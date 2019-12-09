# Working from https://www.embeddeduse.com/2017/06/03/cmake-cross-compilation-based-on-yocto-sdk/
# export SYSROOTS=/media/nort/Yocto/yocto_varsomam33.u14/SDK/sysroots
# I have hard coded that here, since there are other changes involved

set(SYSROOTS /media/nort/Yocto/yocto_varsomam33.u14/SDK/sysroots)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7-a)
set(TOOLCHAIN_PREFIX ${SYSROOTS}/x86_64-arago-linux/usr/bin/arm-linux-gnueabihf-)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_SYSROOT ${SYSROOTS}/cortexa8hf-neon-linux-gnueabi)

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(tmpdir /media/nort/Yocto/yocto_varsomam33.u14/tisdk/build/arago-tmp-external-linaro-toolchain)

set(COMPILER_FLAGS " -march=armv7-a -marm -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a8 -O2 -pipe -g -feliminate-unused-debug-types -fdebug-prefix-map=${tmpdir}/work/x86_64-nativesdk-arago-linux/meta-environment-varsomam33/1.0-r8=/usr/src/debug/meta-environment-varsomam33/1.0-r8 -fdebug-prefix-map=${tmpdir}/sysroots/x86_64-linux= -fdebug-prefix-map=${tmpdir}/sysroots/x86_64-nativesdk-arago-linux= ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMPILER_FLAGS} -isystem/opt/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabihf/arm-linux-gnueabihf/include" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_FLAGS} -isystem/media/nort/Yocto/yocto_varsomam33.u14/SDK/sysroots/cortexa8hf-neon-linux-gnueabi/usr/include/c++/5.3.1 -isystem/media/nort/Yocto/yocto_varsomam33.u14/SDK/sysroots/cortexa8hf-neon-linux-gnueabi/usr/include/c++/5.3.1/arm-linux-gnueabihf" CACHE STRING "" FORCE)

set(CMAKE_STAGING_PREFIX /opt/linkeng/am335x CACHE PATH "Install Directory")

