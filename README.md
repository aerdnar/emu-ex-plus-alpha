# Welcome to How To Compile this project on linux (tested on Ubuntu 16.04)
https://github.com/Rakashazi/emu-ex-plus-alpha 		

---------------------------------------------------------------------------
## Dipendences

* Java

		sudo add-apt-repository ppa:webupd8team/java
		sudo apt-get update
		sudo apt-get install oracle-java8-installer

* Others

		sudo apt-get install libc6:i386 libncurses5:i386 libstdc++6:i386 lib32z1 libbz2-1.0:i386
    
	Maybe
	
		sudo apt-get install liblzma

* Compilers

		sudo apt-get install cmake gcc g++
		sudo apt-get install build-essential
		sudo apt-get install ant pkg-config autoconf autogen
		sudo apt-get install build-essential libtool
		sudo apt-get install texinfo lldb
    
	Maybe
  
		sudo apt-get install clang
		sudo apt-get install xzdec

---------------------------------------------------------------------------
## Downloads

1.	Download and unpack NDK from 
	https://developer.android.com/ndk/downloads/index.html <br />
	Put it in the home folder and rename it as android-ndk

2.	Download and unpack the project from
	https://github.com/Rakashazi/emu-ex-plus-alpha/archive/master.zip <br />
	Put it in the home folder

3.	Download and install android studio from
	https://developer.android.com/studio/index.html <br />
	Put it in the /opt folder and launch `/android-studio/bin/studio.sh`<br />

4.	Download By SDK Manager: <br />
	*	SDK Platforms:	
		*	Android 7.1.1 	SDK 25 	(Optional)
		*	Android 7.0.0 	SDK 24*	(Important)
		*	Android 4.0.3 	SDK 15	(Optional)
		*	Android 2.3.3 	SDK 10*	(Important)
		*	Android 2.3   	SDK 9	(Optional) 
	*	SDK Tools:		
		*	CMake			(Optional)
		*	LLDB			(Optional)
		*	NDK			(Optional)
		*	Android Support Repository	(Important)

---------------------------------------------------------------------------
## Set variables

1)	Get PKG_CONFIG_PATH by: 

          pkg-config --variable pc_path pkg-config

2)	Get IMAGINE_PATH by:

          cd ~/emu-ex-plus-alpha-master/
          pwd
	
3)	Get ANDROID_HOME where there is sdk, `~/Android/Sdk`

4)	Put them in the `~/.profile` like below

5)	Logout and login to make effective the changes because `source ~/.profile` affect current shell only 

          ~/.profile could be change, so check the current file if you are in different environment

    Example my `~/.profile`

          # set PATH so it includes user's private bin directories
          PATH="$HOME/bin:$HOME/.local/bin:$PATH:$IMAGINE_PATH:$PKG_CONFIG_PATH:ANDROID_HOME"
          export IMAGINE_PATH="$HOME/emu-ex-plus-alpha-master/imagine"
          export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig"
          export ANDROID_HOME="$HOME/Android/Sdk"

6)	To avoid bugs be sure that there are FILE pointer:

          cd ~/emu-ex-plus-alpha-master/imagine
          ls --file-type | grep "@"

    The output should be like:

    >	android.mk@ <br />
    >	android-release.mk@ <br />
    >	ios.mk@ <br />
    >	ios-release.mk@ <br />
    >	linux-armv7-pandora.mk@ <br />
    >	linux-armv7-pandora-release.mk@ <br />
    >	linux-x86_64.mk@ <br />
    >	linux-x86_64-release.mk@ <br />
    >	linux-x86.mk@ <br />
    >	linux-x86-release.mk@ <br />
    >	macosx-x86_64.mk@ <br />
    >	ouya.mk@ <br />
    >	ouya-release.mk@ <br />

	If it's empty try to continue or, if there are errors, download <br /> 
	and unzip again the copy get from github.

---------------------------------------------------------------------------
## Let's go

1)	First, something you should know:

	  1) 	make options: 
	        * V=1 it's verbose 
	        * -j4 it's -j< n-core > 
		
	  2)	Ignore dirname's errors, it's a bug of
		    `/emu-ex-plus-alpha-master/imagine/make/setAndroidNDKPath.mk` 
		    when it try to use `which` to find NDK path, but it's ok.
		
                  ANDROID_NDK_PATH ?= $(shell dirname `which ndk-build`)

	  3)	My personal fix, edit	`/emu-ex-plus-alpha-master/imagine/make/shortcut/meta-builds/android.mk` <br />
		    I've tried to edit this variable
	
                  android_sdkToolsPath := $(shell dirname `which android`)
            as
	
                  android_sdkToolsPath := $(HOME)/Android

	  4)	To retray the compilation:
    
                  rm /tmp/imagine-bundle
                  rm ~/imagine-sdk
                  rm ~/emu-ex-plus-alpha-master

2)	Open the Terminal (I've tried Android only)

          cd ~/emu-ex-plus-alpha-master/imagine/bundle/all/
          bash makeAll-android.sh V=1 -j4

    Check if there are errors:
		
          bash makeAll-android.sh V=1 -j4 1>/dev/null | grep -v "dirname"

3)	It's necessary

          cd ~/emu-ex-plus-alpha-master/imagine/	
          make -f android-java.mk install
	
    If there are errors, check `/Android/Sdk/extras/android/m2repository/com/android/support/` and be sure there is `support-v4` folder <br />
    If there is not install `Android Support Repository` in the SDK Manager <br />
    **In this file there is a problem, I've tried my personal fix editing in `/emu-ex-plus-alpha-master/imagine/android-java.mk` 
    these commands, but it's doesn't work when I try to compile GBC.emu** <br/>

          install : main
          install-links : main

    as

          install : all
          install-links : all

4)  Now, it's time to build android version
    1)	**Debug version:**
        1)  Compile image

                  cd ~/emu-ex-plus-alpha-master/imagine/
                  make -f android.mk V=1 -j4
                  make -f android.mk install V=1 -j4

            Check if there are errors:

                  make -f android.mk install V=1 -j4 1>/dev/null | grep -v "dirname"
                
        2) Compile EmuFramework           
            		
                 cd ~/emu-ex-plus-alpha-master/EmuFramework
                 make -f android.mk V=1 -j4
                 make -f android.mk install V=1 -j4

	          Check if there are errors:

                 make -f android.mk install V=1 -j4 1>/dev/null | grep -v "dirname"

    2)	**Release version:**
        1) Compile image

                 cd ~/emu-ex-plus-alpha-master/imagine/
                 make -f android-release.mk V=1 -j4
                 make -f android-release.mk install V=1 -j4

            Check if there are errors:

                 make -f android-release.mk install V=1 -j4 1>/dev/null | grep -v "dirname"	
                
        2)	Compile EmuFramework

                  cd ~/emu-ex-plus-alpha-master/EmuFramework
                  make -f android-release.mk V=1 -j4
                  make -f android-release.mk install V=1 -j4

            Check if there are errors:

                  make -f android-release.mk install V=1 -j4 1>/dev/null | grep -v "dirname"
		
5)	To build android release version (I've tried GBC.emu):
		
          cd ~/emu-ex-plus-alpha-master/GBC.emu
          make -f android-9.mk V=1 -j4 
          make -f android-9.mk install V=1 -j4





---------------------------------------------------------------------------
# Maybe he can help someone

https://www.reddit.com/r/AndroidGaming/comments/32zasy/what_do_you_think_of_robert_broglias_emu_series/cqjslx0/ <br />

Well this thread lit a bit of a fire under my ass. If I am not willing to put a bit of effort into obtaining these emulators the legal way (compiling from source, as per GPL) then I probably don't deserve to stomp on someone else's efforts and rights to profit either. Obviously Robert has put a lot of work into his front end and making it cross platform and he reserves some right to seek compensation from users for that work. <br />
So... I did exactly that. Downloaded the source code to a linux box and painstakingly figured out how to compile it. <br /> Seriously. This took me > 10 hours to get everything worked out so hope you have a comfy chair. <br />
I am not going to do an exact step by step for people but basically: <br />

    You need a linux box 
    Get your GCC version up to 4.9 or higher 
    Get your GNU Make version up to 3.8.2 or higher 
    Install Android SDK 
    Install Android NDK 
    Install JDK 
    Install Ant 
    Install all the other compiling essentials 
Download the source. Compiling the source follows this order of operation: <br />

    Read documentation in Imagine/docs folder 
    Compile + Install Imagine pre-requisites from /imagine/bundle/all/ 
    Compile + Install Imagine 
    Compile + Install EmuFramework 
    Compile the emulator you want to use 
    Sign the APK 
I'm hugely over simplifying things. Good luck to all who set out to accomplish this starting from little or no knowledge of compiling c++ based android apps. <br />

---------------------------------------------------------------------------
