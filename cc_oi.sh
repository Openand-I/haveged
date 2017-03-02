#!/bin/bash

#      _           _      _             _     _
#     | |_   _ ___| |_   / \   _ __ ___| |__ (_)
#  _  | | | | / __| __| / _ \ | '__/ __| '_ \| |
# | |_| | |_| \__ \ |_ / ___ \| | | (__| | | | |
#  \___/ \__,_|___/\__/_/   \_\_|  \___|_| |_|_|
#
# Copyright 2014 Łukasz "JustArchi" Domeradzki
# Contact: JustArchi@JustArchi.net
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#############
### BASIC ###
#############

# Root of NDK, the one which contains $NDK/ndk-build binary
NDK="$HOME/android-ndk-r12b"
export GOOGLE_PLATFORM=19
export NDK_TOOLCHAIN_VERSION=4.9

ANDROID_NATIVE_API_LEVEL="android-${GOOGLE_PLATFORM}"

# apt-get install gcc-arm-linux-gnueabi
# Download NDK
# Root of NDK toolchain, the one used in --install-dir from $NDK/build/tools/make-standalone-toolchain.sh. Make sure it contains $NDKTC/bin directory with $CROSS_COMPILE binaries
# Original command: build/tools/make-standalone-toolchain.sh --toolchain=arm-linux-androideabi-4.9 --platform=android-19 --install-dir=$HOME/ndk
#NDKTC="$HOME/ndk"
HOST_ARCH=$(uname -m)
NDKTC="${NDK}/toolchains/arm-linux-androideabi-${NDK_TOOLCHAIN_VERSION}/prebuilt/linux-${HOST_ARCH}"

# Optional, may help NDK in some cases, should be equal to GCC version of the toolchain specified above
export SYSROOT="${NDK}/platforms/${ANDROID_NATIVE_API_LEVEL}/arch-arm"

# This flag turns on ADVANCED section below, you should use "0" if you want easy compiling for generic targets, or "1" if you want to get best optimized results for specific targets
# In general it's strongly suggested to leave it turned on, but if you're using makefiles, which already specify optimization level and everything else, then of course you may want to turn it off

ADVANCED="1"

################
### ADVANCED ###
################

# Device CFLAGS, these should be taken from TARGET_GLOBAL_CFLAGS property of BoardCommonConfig.mk of your device, eventually leave them empty for generic non-device-optimized build
# Please notice that -march flag comes from TARGET_ARCH_VARIANT

DEVICECFLAGS=" -marm -march=armv5te -msoft-float -mfpu=vfp -mfloat-abi=softfp "

# This specifies optimization level used during compilation. Usually it's a good idea to keep it on "-O2" for best results, but you may want to experiment with "-Os", "-O3" or "-Ofast"

OLEVEL=" -O0 "

# This specifies extra optimization flags, which are not selected by any of optimization levels chosen above
# Please notice that they're pretty EXPERIMENTAL, and if you get any compilation errors, the first step is experimenting with them or disabling them completely, you may also want to try different O level
# Try removing flto if 'recompile with -fPIC' error or '.o undefined reference to' error

OPTICFLAGS=" -I${SYSROOT}/usr/include -fno-short-enums -fgcse-after-reload -frename-registers "
OPTICFLAGS+=" -funsafe-loop-optimizations -fivopts -ftree-loop-im -ftree-loop-ivcanon -ffunction-sections -fdata-sections -funswitch-loops -frerun-cse-after-loop -fomit-frame-pointer -fgcse-after-reload -fgcse-sm -fgcse-las -fweb -ftracer -s -fstrict-aliasing -fvisibility=hidden -fstack-protector -D_FORTIFY_SOURCE=2 -fwrapv -Wstack-protector -Wformat -Wformat-security "

# This specifies extra linker optimizations. Same as above, in case of problems this is second step for finding out the culprit
# Try removing flto if 'recompile with -fPIC' error or '.o undefined reference to' error

#LDFLAGS=" -Xlinker -z -Xlinker muldefs -nostdlib -Bdynamic -Xlinker -dynamic-linker -Xlinker /system/bin/linker -Xlinker -z -Xlinker nocopyreloc -Xlinker --no-undefined -L${SYSROOT}/usr/lib -fuse-ld=bfd "
#LDFLAGS=" -Xlinker -z -Xlinker muldefs -Bdynamic -Xlinker -dynamic-linker -Xlinker /system/bin/linker -Xlinker -z -Xlinker nocopyreloc -Xlinker --no-undefined -L${SYSROOT}/usr/lib -fuse-ld=bfd "
LDFLAGS=" -Xlinker -z -Xlinker muldefs -Xlinker -z -Xlinker nocopyreloc -Xlinker --no-undefined -L${SYSROOT}/usr/lib -fuse-ld=bfd "
LDFLAGS+=" -Wl,-O0 -Wl,--as-needed -Wl,--relax -Wl,-z,relro -Wl,-z,now "

# This specifies additional sections to strip, for extra savings on size

STRIPFLAGS=" -s -R .note -R .comment "

# Additional definitions, which may help some binaries to work with android

DEFFLAGS=" -fPIC -DPIC -DNDEBUG -DANDROID -D__ANDROID__ -DSK_RELEASE -DLINUX "

##############
### EXPERT ###
##############

# This specifies host (target) for makefiles. In some rare scenarios you may also try "--host=arm-linux-androideabi"
# In general you shouldn't change that, as you're compiling binaries for low-level ARM-EABI and not Android itself

CONFIGANDROID="--host=arm-linux-androideabi --with-sysroot=$SYSROOT"

# This specifies the CROSS_COMPILE variable, again, in some rare scenarios you may also try "arm-eabi-"
# But beware, NDK doesn't even offer anything apart from arm-linux-androideabi one, however custom toolchains such as Linaro offer arm-eabi as well

CROSS_COMPILE="arm-linux-androideabi-"

# This specifies if we should also override our native toolchain in the PATH in addition to overriding makefile commands such as CC
# You should NOT enable it, unless your makefile calls "gcc" instead of "$CC" and you want to point "gcc" (and similar) to NDKTC
# However, in such case, you should either fix makefile yourself or not use it at all
# You've been warned, this is not a good idea

TCOVERRIDE="0"

# Workaround for some broken compilers with malloc problems (undefined reference to rpl_malloc and similar errors during compiling), don't uncomment unless you need it
#export ac_cv_func_malloc_0_nonnull=yes

############
### CORE ###
############

# You shouldn't edit anything from now on
if [ "$ADVANCED" -ne 0 ]; then # If advanced is specified, we override flags used by makefiles with our optimized ones, of course if makefile allows that
	export CFLAGS="$OLEVEL $DEVICECFLAGS $OPTICFLAGS $DEFFLAGS --sysroot=$SYSROOT"
	export LOCAL_CFLAGS="$CFLAGS"
	export CXXFLAGS="$CFLAGS" # We use same flags for CXX as well
	export LOCAL_CXXFLAGS="$CXXFLAGS"
	export CPPFLAGS="$CXXFLAGS" # Yes, CPP is the same as CXX, because they're both used in different makefiles/compilers, unfortunately
	export LOCAL_CPPFLAGS="$CPPFLAGS"
	export LDFLAGS="$LDFLAGS"
	export LOCAL_LDFLAGS="$LDFLAGS"
fi

if [ ! -z "$NDK" ] && [ "$(echo $PATH | grep -qi $NDK; echo $?)" -ne 0 ]; then # If NDK doesn't exist in the path (yet), prepend it
        export PATH="$NDK:$PATH"
fi

if [ ! -z "$NDKTC" ] && [ "$(echo $PATH | grep -qi $NDKTC; echo $?)" -ne 0 ]; then # If NDKTC doesn't exist in the path (yet), prepend it
        export PATH="$NDKTC/bin:$PATH"
fi

export CROSS_COMPILE="$CROSS_COMPILE" # All makefiles depend on CROSS_COMPILE variable, this is important to set"
export AS=${CROSS_COMPILE}as
export AR=${CROSS_COMPILE}ar
export CC=${CROSS_COMPILE}gcc
export CXX=${CROSS_COMPILE}g++
export CPP=${CROSS_COMPILE}cpp
export LD=${CROSS_COMPILE}ld
export NM=${CROSS_COMPILE}nm
export OBJCOPY=${CROSS_COMPILE}objcopy
export OBJDUMP=${CROSS_COMPILE}objdump
export READELF=${CROSS_COMPILE}readelf
export RANLIB=${CROSS_COMPILE}ranlib
export SIZE=${CROSS_COMPILE}size
export STRINGS=${CROSS_COMPILE}strings
export STRIP=${CROSS_COMPILE}strip

if [ "$TCOVERRIDE" -eq 1 ]; then # This is not a a good idea...
	alias as="$AS"
	alias ar="$AR"
	alias gcc="$CC"
	alias g++="$CXX"
	alias cpp="$CPP"
	alias ld="$LD"
	alias nm="$NM"
	alias objcopy="$OBJCOPY"
	alias objdump="$OBJDUMP"
	alias readelf="$READELF"
	alias ranlib="$RANLIB"
	alias size="$SIZE"
	alias strings="$STRINGS"
	alias strip="$STRIP"
fi

export CONFIGANDROID="$CONFIGANDROID"
export CCC="$CC $CFLAGS $LDFLAGS"
export CXX="$CXX $CXXFLAGS $LDFLAGS"
export SSTRIP="$STRIP $STRIPFLAGS"

echo "Done setting your environment"
echo
echo "CFLAGS: $CFLAGS"
echo "LDFLAGS: $LDFLAGS"
echo "CC points to $CC and this points to $(which "$CC")"
echo
echo "Use \"\$CC\" command for calling gcc and \"\$CCC\" command for calling our optimized CC"
echo "Use \"\$CXX\" command for calling g++ and \"\$CCXX\" for calling our optimized CXX"
echo "Use \"\$STRIP\" command for calling strip and \"\$SSTRIP\" command for calling our optimized STRIP"
echo
echo "Example: \"\$CCC myprogram.c -o mybinary && \$SSTRIP mybinary \""
echo
echo "When using makefiles with configure options, always use \"./configure \$CONFIGANDROID\" instead of using \"./configure\" itself"
echo "Please notice that makefiles may, or may not, borrow our CFLAGS and LFLAGS, so I suggest to double-check them and eventually append them to makefile itself"
echo "Pro tip: Makefiles with configure options always borrow CC, CFLAGS and LDFLAGS, so if you're using ./configure, probably you don't need to do anything else"
