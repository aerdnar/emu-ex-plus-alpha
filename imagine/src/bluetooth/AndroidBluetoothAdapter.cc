/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#define thisModuleName "android-bt"
#include "AndroidBluetoothAdapter.hh"
#include <util/fd-utils.h>
#include <errno.h>
#include <ctype.h>
#include <util/collection/DLList.hh>
#include <base/android/private.hh>
#include "utils.hh"

static AndroidBluetoothAdapter defaultAndroidAdapter;

static JavaInstMethod<jint> jStartScan, jInRead;
static JavaInstMethod<jobject> jDefaultAdapter, jOpenSocket, jBtSocketInputStream, jBtSocketOutputStream;
static JavaInstMethod<void> jBtSocketClose, jOutWrite;

// runs in activity thread
static void JNICALL btScanStatus(JNIEnv* env, jobject thiz, jint res)
{
	logMsg("scan complete");
	Base::sendBTScanStatusDelegate(AndroidBluetoothAdapter::SCAN_COMPLETE);
	defaultAndroidAdapter.inDetect = 0;
}

// runs in activity thread
static jboolean JNICALL scanDeviceClass(JNIEnv* env, jobject thiz, jint classInt)
{
	logMsg("got class %X", classInt);
	uchar classByte[3];
	classByte[2] = classInt >> 16;
	classByte[1] = (classInt >> 8) & 0xff;
	classByte[0] = classInt & 0xff;
	if(!defaultAndroidAdapter.scanDeviceClassDelegate().invoke(classByte))
	{
		logMsg("skipping device due to class %X:%X:%X", classByte[0], classByte[1], classByte[2]);
		return 0;
	}
	return 1;
}

// runs in activity thread
static void JNICALL scanDeviceName(JNIEnv* env, jobject thiz, jstring name, jstring addr)
{
	const char *nameStr = env->GetStringUTFChars(name, 0);
	BluetoothAddr addrByte;
	{
		const char *addrStr = env->GetStringUTFChars(addr, 0);
		str2ba(addrStr, &addrByte);
		env->ReleaseStringUTFChars(addr, addrStr);
	}
	defaultAndroidAdapter.scanDeviceNameDelegate().invoke(nameStr, addrByte);
	env->ReleaseStringUTFChars(name, nameStr);
}

bool AndroidBluetoothAdapter::openDefault()
{
	if(adapter)
		return 1;

	using namespace Base;
	// setup JNI
	if(jDefaultAdapter.m == 0)
	{
		logMsg("JNI setup");
		jDefaultAdapter.setup(eEnv(), jBaseActivityCls, "btDefaultAdapter", "()Landroid/bluetooth/BluetoothAdapter;");
		jStartScan.setup(eEnv(), jBaseActivityCls, "btStartScan", "(Landroid/bluetooth/BluetoothAdapter;)I");
		jOpenSocket.setup(eEnv(), jBaseActivityCls, "btOpenSocket", "(Landroid/bluetooth/BluetoothAdapter;Ljava/lang/String;IZ)Landroid/bluetooth/BluetoothSocket;");

		jclass jBluetoothSocketCls = eEnv()->FindClass("android/bluetooth/BluetoothSocket");
		assert(jBluetoothSocketCls);
		jBtSocketClose.setup(eEnv(), jBluetoothSocketCls, "close", "()V");
		jBtSocketInputStream.setup(eEnv(), jBluetoothSocketCls, "getInputStream", "()Ljava/io/InputStream;");
		jBtSocketOutputStream.setup(eEnv(), jBluetoothSocketCls, "getOutputStream", "()Ljava/io/OutputStream;");

		jclass jInputStreamCls = eEnv()->FindClass("java/io/InputStream");
		assert(jInputStreamCls);
		jInRead.setup(eEnv(), jInputStreamCls, "read", "([BII)I");

		jclass jOutputStreamCls = eEnv()->FindClass("java/io/OutputStream");
		assert(jOutputStreamCls);
		jOutWrite.setup(eEnv(), jOutputStreamCls, "write", "([BII)V");

		static JNINativeMethod activityMethods[] =
		{
		    {"onBTScanStatus", "(I)V", (void *)&btScanStatus},
		    {"onScanDeviceClass", "(I)Z", (void *)&scanDeviceClass},
		    {"onScanDeviceName", "(Ljava/lang/String;Ljava/lang/String;)V", (void *)&scanDeviceName},
		};

		eEnv()->RegisterNatives(jBaseActivityCls, activityMethods, sizeofArray(activityMethods));
	}

	logMsg("opening default BT adapter, %p", jBaseActivity);
	adapter = jDefaultAdapter(eEnv(), jBaseActivity);
	if(!adapter)
	{
		logErr("error opening adapter");
		return 0;
	}
	logMsg("success %p", adapter);
	adapter = eEnv()->NewGlobalRef(adapter);

	return 1;
}

void AndroidBluetoothAdapter::close()
{
	inDetect = 0;
}

AndroidBluetoothAdapter *AndroidBluetoothAdapter::defaultAdapter()
{
	if(defaultAndroidAdapter.openDefault())
		return &defaultAndroidAdapter;
	else
		return nullptr;
}

fbool AndroidBluetoothAdapter::startScan()
{
	using namespace Base;
	if(!inDetect)
	{
		inDetect = 1;
		logMsg("start scan");
		if(!jStartScan(eEnv(), jBaseActivity, adapter))
		{
			inDetect = 0;
			logMsg("failed to start scan");
			return 0;
		}
		logMsg("started scan");
		return 1;
	}
	else
	{
		logMsg("previous bluetooth detection still running");
		return 0;
	}
}

void AndroidBluetoothAdapter::constructSocket(void *mem)
{
	new(mem) AndroidBluetoothSocket();
}

void* AndroidBluetoothSocket::readThreadFunc(void *arg)
{
	logMsg("in read thread");
	#if CONFIG_ENV_ANDROID_MINSDK >= 9
	JNIEnv *jEnv;
	if(Base::jVM->AttachCurrentThread(&jEnv, 0) != 0)
	{
		logErr("error attaching jEnv to thread");
		return 0;
	}
	#endif

	auto &s = *((AndroidBluetoothSocket*)arg);
	jbyteArray jData = jEnv->NewByteArray(48);
	jboolean jDataArrayIsCopy;
	jbyte *data = jEnv->GetByteArrayElements(jData, &jDataArrayIsCopy);
	if(unlikely(jDataArrayIsCopy)) // can't get direct pointer to memory
	{
		jEnv->ReleaseByteArrayElements(jData, data, 0);
		logErr("couldn't get direct array pointer");
		Base::sendBTSocketStatusDelegate(s, STATUS_ERROR);
		#if CONFIG_ENV_ANDROID_MINSDK >= 9
		Base::jVM->DetachCurrentThread();
		#endif
		return nullptr;
	}
	jobject jInput = jBtSocketInputStream(jEnv, s.socket);
	for(;;)
	{
		int len = jInRead(jEnv, jInput, jData, 0, 48);
		//logMsg("read %d bytes", len);
		if(unlikely(len <= 0 || jEnv->ExceptionOccurred()))
		{
			logMsg("error reading packet from in stream %p", jInput);
			jEnv->ExceptionClear();
			Base::sendBTSocketStatusDelegate(s, STATUS_ERROR);
			break;
		}
		Base::sendBTSocketData(s, len, data);
	}

	jEnv->ReleaseByteArrayElements(jData, data, 0);

	#if CONFIG_ENV_ANDROID_MINSDK >= 9
	Base::jVM->DetachCurrentThread();
	#endif

	return nullptr;
}

CallResult AndroidBluetoothSocket::openSocket(BluetoothAddr bdaddr, uint channel, bool l2cap)
{
	JNIEnv *jEnv;
	Base::jVM->GetEnv((void**) &jEnv, JNI_VERSION_1_6);
	char addrStr[18];
	ba2str(&bdaddr, addrStr);
	//jbyteArray jAddr = jEnv->NewByteArray(sizeof(BluetoothAddr));
	//jEnv->SetByteArrayRegion(jAddr, 0, sizeof(BluetoothAddr), (jbyte *)bdaddr.b);
	socket = jEnv->NewGlobalRef(jOpenSocket(jEnv, Base::jBaseActivity,
		AndroidBluetoothAdapter::defaultAdapter()->adapter, jEnv->NewStringUTF(addrStr), channel, l2cap ? 1 : 0));
	if(socket)
	{
		logMsg("opened Bluetooth socket %p", socket);
		outStream = jEnv->NewGlobalRef(jBtSocketOutputStream(jEnv, socket));
		if(onStatus.invoke(*this, STATUS_OPENED) == REPLY_OPENED_USE_READ_EVENTS)
		{
			logMsg("starting read thread");
			readThread.create(1, readThreadFunc, this);
		}
		return OK;
	}
	Base::sendBTScanStatusDelegate(BluetoothAdapter::SOCKET_OPEN_FAILED);
	return IO_ERROR;
}

CallResult AndroidBluetoothSocket::openRfcomm(BluetoothAddr bdaddr, uint channel)
{
	logMsg("opening RFCOMM channel %d", channel);
	return openSocket(bdaddr, channel, 0);
}

CallResult AndroidBluetoothSocket::openL2cap(BluetoothAddr bdaddr, uint psm)
{
	logMsg("opening L2CAP psm %d", psm);
	return openSocket(bdaddr, psm, 1);
}

void AndroidBluetoothSocket::close()
{
	if(socket)
	{
		logMsg("closing socket");
		JNIEnv *jEnv;
		Base::jVM->GetEnv((void**) &jEnv, JNI_VERSION_1_6);
		jEnv->DeleteGlobalRef(outStream);
		jBtSocketClose(jEnv, socket);
		jEnv->DeleteGlobalRef(socket);
		socket = nullptr;
	}
}

CallResult AndroidBluetoothSocket::write(const void *data, size_t size)
{
	logMsg("writing %d bytes", size);
	JNIEnv *jEnv;
	Base::jVM->GetEnv((void**) &jEnv, JNI_VERSION_1_6);
	jbyteArray jData = jEnv->NewByteArray(size);
	jEnv->SetByteArrayRegion(jData, 0, size, (jbyte *)data);
	jOutWrite(jEnv, outStream, jData, 0, size);
	jEnv->DeleteLocalRef(jData);
	return OK;
}