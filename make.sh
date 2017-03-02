#!/bin/bash


if [ -f $HOME/toolchain/cc_tmp.sh ]; then 
  . $HOME/toolchain/cc_tmp.sh
fi

#LDFLAGS="$LDFLAGS $1" CFLAGS="$CFLAGS -D_THREAD_SAFE -DSIGHUP $2" ./configure $CONFIGANDROID $3 --enable-daemon=yes --enable-olt=yes --enable-diagnostic=no

#./configure LIBS="-lc -lm" $CONFIGANDROID --enable-daemon=yes --enable-olt=yes --enable-diagnostic=no --enable-clock_gettime=yes

#./configure LIBS="-lm" $CONFIGANDROID --enable-daemon=yes --enable-olt=yes --enable-diagnostic=no --enable-clock_gettime=yes --enable-init=service.redhat --enable-tune=yes --enable-nistest=yes

#LDFLAGS+='-static' ./configure LIBS="-lm" $CONFIGANDROID --enable-daemon=yes --enable-olt=no --enable-diagnostic=no --enable-clock_gettime=yes --enable-init=service.redhat --enable-tune=vfs --enable-nistest=yes

LDFLAGS+='-static' ./configure LIBS="-lm" $CONFIGANDROID --enable-daemon=yes --enable-olt=yes --enable-clock_gettime=yes --enable-init=service.redhat --enable-tune=yes --enable-nistest=yes

if [ $? != 0 ]; then exit 1; fi

#if [ "x$CONFIGANDROID" != "x" ]; then 
#  sed -e '/lt_prog_compiler_pic=/s/^/#/g' -i config.status
#fi

#make clean ; make CFLAGS+='-static' LDFLAGS+='-static'
#make clean ; make LDFLAGS+='-static'
make clean ; make

# Below is to make the static exe. Remove the -nostdlib option if present
# arm-linux-androideabi-gcc -Wall -I.. -O3 -march=armv5te -msoft-float -mfloat-abi=softfp -mfpu=vfp -mthumb -mthumb-interwork -I/home/annitb/android-ndk-r10e/platforms/android-19/arch-arm/usr/include -fno-short-enums -fgcse-after-reload -frename-registers -funsafe-loop-optimizations -fivopts -ftree-loop-im -ftree-loop-ivcanon -ffunction-sections -fdata-sections -funswitch-loops -frerun-cse-after-loop -fomit-frame-pointer -fgcse-after-reload -fgcse-sm -fgcse-las -fweb -ftracer -s -fstrict-aliasing -fvisibility=hidden -fstack-protector -D_FORTIFY_SOURCE=2 -fwrapv -Wstack-protector -Wformat -Wformat-security -fPIC -DPIC -DNDEBUG -DANDROID -D__ANDROID__ -DSK_RELEASE -DLINUX --sysroot=/home/annitb/android-ndk-r10e/platforms/android-19/arch-arm -Wl,-z -Wl,muldefs -Bdynamic -Wl,-dynamic-linker -Wl,/system/bin/linker -Wl,-z -Wl,nocopyreloc -Wl,--no-undefined -fuse-ld=bfd -Wl,-O3 -Wl,--as-needed -Wl,--relax -Wl,-z -Wl,relro -Wl,-z -Wl,now -o haveged haveged.o  -L/home/annitb/android-ndk-r10e/platforms/android-19/arch-arm/usr/lib ./.libs/libhavege.a -static

cd src

if [ "x$CCC" = "x" ]; then
  CCC="gcc"   
fi;

if [ "x$SSTRIP" = "x" ]; then 
  SSTRIP="strip" 
fi;

$SSTRIP haveged

$CCC haveged.o .libs/libhavege.a -o haveged.static -static

$SSTRIP haveged.static

if [ "x$CONFIGANDROID" != "x" ]; then
  cp haveged.static android/haveged
  cp haveged.static android/haveged.static
  cp haveged android/haveged.dynamic
  cp haveged android/haveged.dynamic.pie
  elfedit --output-type DYN android/haveged.dynamic.pie
  cd android
else
  cp haveged.static linux/haveged
  cp haveged.static linux/haveged.static
  cp haveged linux/haveged.dynamic
  cd linux
fi

#if [ -x `which upx` ]; then upx -k --ultra-brute haveged; fi

if [ -x `which upx` ]; then upx haveged; fi

rm -f haveged.0*