#!/bin/bash
# Cross compile for windows from Linux:
#
# Known to work at Ubuntu 23.04
set -e

if ! test -f ./configure || test $# != 1; then echo "Please execute from the main project directory like this:"; echo "mingw/build.sh x86_64-w64-mingw32    (or i686-w64-mingw32 for a 32bit build)"; exit 1; fi
if test "$1" != "x86_64-w64-mingw32" && test "$1" != "i686-w64-mingw32"; then echo "only supported targets are x86_64-w64-mingw32 and i686-w64-mingw32. $1 given"; exit 1; fi

test -f ./Makefile && make clean || true

sudo apt-get install mingw-w64-tools mingw-w64 libz-mingw-w64-dev nasm cmake
host=$1
install=`pwd`/$host-install
prefix=`pwd`/$host-prefix
mkdir -p $host-src
mkdir -p $host-prefix/lib
mkdir -p $host-prefix/include
cd $host-src

generate_toolchain_cmake () {
  if test "$host" == "i686-w64-mingw32"; then
    cat <<EOF > toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR X86)
set(CMAKE_C_COMPILER /usr/bin/i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER /usr/bin/i686-w64-mingw32-windres)
EOF
  else
    cat <<EOF > toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)
EOF
  fi
}

do_cmake () {
  generate_toolchain_cmake
  mkdir -p build
  cd build
  PKG_CONFIG=$host-pkg-config \
  PKG_CONFIG_PATH=$prefix/lib/pkgconfig/ \
  cmake -G"Unix Makefiles" \
        -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake \
        -DCMAKE_INSTALL_PREFIX=$prefix \
        -DCMAKE_INCLUDE_PATH=$prefix/include \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_BUILD_TYPE=Release \
        "$@" \
        ..
  make all install
  cd ..
}

#wget https://zlib.net/zlib-1.2.13.tar.gz
#tar xfz zlib-1.2.13.tar.gz
#cd zlib-1.2.13/
#prefix=$prefix CC=$host-gcc CFLAGS="-O2" ./configure
#make
#cd ..

########## BZIP2 ##########
wget https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz -O bzip2-1.0.8.tar.gz
rm -Rf bzip2-1.0.8
tar xfz bzip2-1.0.8.tar.gz
cd bzip2-1.0.8
cat <<EOF > Makefile.mingw32
CC=$host-gcc
CFLAGS=-fpic -fPIC -DWIN32 -MD -O2 -D_FILE_OFFSET_BITS=64 -DBZ_EXPORT

OBJS= blocksort.o  \\
      huffman.o    \\
      crctable.o   \\
      randtable.o  \\
      compress.o   \\
      decompress.o \\
      bzlib.o

all: bz2.dll bzip2-shared.exe

bz2.dll: \$(OBJS)
	\$(CC) -shared -Wl,--subsystem,windows -o \$@ $^

bzip2-shared.exe: bz2.dll bzip2.c
	\$(CC) \$(CFLAGS) -o \$@ bzip2.c bz2.dll

clean:
	rm -f *.o *.a *.dll *.exe

blocksort.o: blocksort.c
	\$(CC) \$(CFLAGS) -c blocksort.c
huffman.o: huffman.c
	\$(CC) \$(CFLAGS) -c huffman.c
crctable.o: crctable.c
	\$(CC) \$(CFLAGS) -c crctable.c
randtable.o: randtable.c
	\$(CC) \$(CFLAGS) -c randtable.c
compress.o: compress.c
	\$(CC) \$(CFLAGS) -c compress.c
decompress.o: decompress.c
	\$(CC) \$(CFLAGS) -c decompress.c
bzlib.o: bzlib.c
	\$(CC) \$(CFLAGS) -c bzlib.c
EOF
 patch -p 1 <<EOF
--- bzip2-1.0.8/bzlib.h	2019-07-13 19:50:05.000000000 +0200
+++ bzip2-1.0.8-new/bzlib.h	2023-06-11 01:30:31.804371512 +0200
@@ -66,8 +66,8 @@
    bz_stream;
 
 
-#ifndef BZ_IMPORT
-#define BZ_EXPORT
+#ifndef BZ_EXPORT
+#define BZ_IMPORT
 #endif
 
 #ifndef BZ_NO_STDIO
@@ -82,12 +82,12 @@
 #      undef small
 #   endif
 #   ifdef BZ_EXPORT
-#   define BZ_API(func) WINAPI func
-#   define BZ_EXTERN extern
+#   define BZ_API(func) __cdecl func
+#   define BZ_EXTERN extern __declspec(dllexport)
 #   else
    /* import windows dll dynamically */
-#   define BZ_API(func) (WINAPI * func)
-#   define BZ_EXTERN
+#   define BZ_API(func) __cdecl func
+#   define BZ_EXTERN extern __declspec(dllimport)
 #   endif
 #else
 #   define BZ_API(func) func
EOF
 make -f Makefile.mingw32
 cp bz2.dll $prefix/lib
 cp bzlib.h $prefix/include
 cd ..

########## MAD ##########
wget https://sourceforge.net/projects/mad/files/libmad/0.15.1b/libmad-0.15.1b.tar.gz/download -O libmad-0.15.1b.tar.gz
rm -Rf libmad-0.15.1b
tar xfz libmad-0.15.1b.tar.gz
cd libmad-0.15.1b
patch -p 1 << EOF
diff -u libmad-0.15.1b/configure.ac libmad-0.15.1b-new/configure.ac
--- libmad-0.15.1b/configure.ac	2004-01-23 10:41:32.000000000 +0100
+++ libmad-0.15.1b-new/configure.ac	2023-04-07 21:37:26.401404106 +0200
@@ -51,11 +51,11 @@
 	    esac
     esac
 
-dnl    case "\$host" in
-dnl	*-*-cygwin* | *-*-mingw*)
-dnl	    LDFLAGS="\$LDFLAGS -no-undefined -mdll"
-dnl	    ;;
-dnl    esac
+    case "\$host" in
+	*-*-cygwin* | *-*-mingw*)
+	    LDFLAGS="\$LDFLAGS -no-undefined"
+	    ;;
+    esac
 fi
 
 dnl Support for libtool.
@@ -140,7 +140,7 @@
     case "\$optimize" in
 	-O|"-O "*)
 	    optimize="-O"
-	    optimize="\$optimize -fforce-mem"
+	    #optimize="\$optimize -fforce-mem"
 	    optimize="\$optimize -fforce-addr"
 	    : #x optimize="\$optimize -finline-functions"
 	    : #- optimize="\$optimize -fstrength-reduce"
EOF
touch AUTHORS ChangeLog NEWS
autoreconf --install
./configure --host $host --prefix=$prefix
make all install
#cp .libs/libmad-0.dll $prefix/lib/mad.dll
#cp mad.h $prefix/include
cd ..

########## LIBJPEG-TURBO ##########
wget https://sourceforge.net/projects/libjpeg-turbo/files/3.0.0/libjpeg-turbo-3.0.0.tar.gz/download -O libjpeg-turbo-3.0.0.tar.gz
rm -Rf libjpeg-turbo-3.0.0
tar xfz libjpeg-turbo-3.0.0.tar.gz
cd libjpeg-turbo-3.0.0
do_cmake -DENABLE_SHARED=TRUE
cd ..

########## LIBPNG ##########
wget https://download.sourceforge.net/libpng/libpng-1.6.40.tar.gz -O libpng-1.6.40.tar.gz
rm -Rf libpng-1.6.40
tar xfz libpng-1.6.40.tar.gz
cd libpng-1.6.40
#cmake does not use pkg-config for zlib?????
do_cmake -DZLIB_ROOT=/usr/$host/
cd ..

########## LIBOGG ##########
wget https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz -O libogg-1.3.5.tar.gz
rm -Rf libogg-1.3.5/
tar xfz libogg-1.3.5.tar.gz
cd libogg-1.3.5/
do_cmake
cd ..
# Bug in build-system, missmatch with dll filename, and internal name
mv -f $prefix/bin/libogg.dll $prefix/bin/ogg.dll

########## VORBIS ##########
wget https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.gz -O libvorbis-1.3.7.tar.gz
rm -Rf libvorbis-1.3.7
tar xfz libvorbis-1.3.7.tar.gz
cd libvorbis-1.3.7
patch -p1 << EOF
diff -ur libvorbis-1.3.7/win32/vorbis.def libvorbis-1.3.7-new/win32/vorbis.def
--- libvorbis-1.3.7/win32/vorbis.def	2020-03-23 16:04:43.000000000 +0100
+++ libvorbis-1.3.7-new/win32/vorbis.def	2023-04-09 10:02:45.550479019 +0200
@@ -1,6 +1,6 @@
 ; vorbis.def
 ; 
-LIBRARY
+LIBRARY vorbis
 EXPORTS
 _floor_P
 _mapping_P
diff -ur libvorbis-1.3.7/win32/vorbisenc.def libvorbis-1.3.7-new/win32/vorbisenc.def
--- libvorbis-1.3.7/win32/vorbisenc.def	2020-03-23 16:04:43.000000000 +0100
+++ libvorbis-1.3.7-new/win32/vorbisenc.def	2023-04-09 10:01:52.862206991 +0200
@@ -1,6 +1,6 @@
 ; vorbisenc.def
 ;
-LIBRARY
+LIBRARY vorbisenc
 
 EXPORTS
 vorbis_encode_init
diff -ur libvorbis-1.3.7/win32/vorbisfile.def libvorbis-1.3.7-new/win32/vorbisfile.def
--- libvorbis-1.3.7/win32/vorbisfile.def	2020-03-23 16:04:43.000000000 +0100
+++ libvorbis-1.3.7-new/win32/vorbisfile.def	2023-04-09 10:01:52.862206991 +0200
@@ -1,6 +1,6 @@
 ; vorbisfile.def
 ;
-LIBRARY
+LIBRARY vorbisfile
 EXPORTS
 ov_clear
 ov_open
EOF
do_cmake
cd ..
# Bug in build-system, missmatch with dll filename, and internal name
mv -f $prefix/bin/libvorbisfile.dll $prefix/bin/vorbisfile.dll
mv -f $prefix/bin/libvorbis.dll $prefix/bin/vorbis.dll

########## FLAC ##########
wget https://ftp.osuosl.org/pub/xiph/releases/flac/flac-1.4.3.tar.xz -O flac-1.4.3.tar.xz
rm -Rf flac-1.4.3
tar xfJ flac-1.4.3.tar.xz
cd flac-1.4.3
do_cmake -D_OGG_LIBRARY_DIRS=$prefix/lib
cd ..

########## SDL2 ##########
wget https://github.com/libsdl-org/SDL/releases/download/release-2.28.1/SDL2-devel-2.28.1-mingw.tar.gz -O SDL2-devel-2.28.1-mingw.tar.gz
rm -Rf SDL2-2.28.1
tar xfz SDL2-devel-2.28.1-mingw.tar.gz
cd SDL2-2.28.1
cp $host/* $prefix -R
prefix2=`echo $prefix|sed -e 's/\//\\\\\//'g`
sed -e "s/^prefix=.*/prefix=$prefix2/" -i $prefix/lib/pkgconfig/sdl2.pc
cd ..

if test "$host" == "i686-w64-mingw32"; then
########## BROTLI ##########, only needed for 32bit, unsure why
  wget https://github.com/google/brotli/archive/refs/tags/v1.0.9.tar.gz -O brotli-1.0.9.tar.gz
  rm -Rf brotli-1.0.9
  tar xfz brotli-1.0.9.tar.gz
  cd brotli-1.0.9
  do_cmake -DCMAKE_CXX_FLAGS=-Wa,-mbig-obj
  cd ..
fi

########## HARFBUZZ #########
wget https://github.com/harfbuzz/harfbuzz/releases/download/8.0.1/harfbuzz-8.0.1.tar.xz -O harfbuzz-8.0.1.tar.xz
rm -Rf harfbuzz-8.0.1
tar xfJ harfbuzz-8.0.1.tar.xz
cd harfbuzz-8.0.1
do_cmake -DCMAKE_CXX_FLAGS=-Wa,-mbig-obj
cd ..

########## FREETYPE2 ##########
wget https://sourceforge.net/projects/freetype/files/freetype2/2.13.1/freetype-2.13.1.tar.gz/download -O freetype-2.13.1.tar.gz
rm -Rf freetype-2.13.1
tar xfz freetype-2.13.1.tar.gz
cd freetype-2.13.1
do_cmake -DZLIB_ROOT=/usr/$host/ \
         -DFT_DISABLE_BZIP2=TRUE
cd ..

########## LIBDISCID ##########
wget http://ftp.eu.metabrainz.org/pub/musicbrainz/libdiscid/libdiscid-0.6.4-win.zip -O libdiscid-0.6.4-win.zip
rm -Rf libdiscid-0.6.4-win
unzip libdiscid-0.6.4-win.zip
if test "$host" == "i686-w64-mingw32"; then
  cp -r libdiscid-0.6.4-win/Win32/* $prefix/lib
else
  cp -r libdiscid-0.6.4-win/x64/* $prefix/lib
fi
cp -r libdiscid-0.6.4-win/include/discid $prefix/include
cat <<EOF > $prefix/lib/pkgconfig/libdiscid.pc
prefix=
exec_prefix=
libdir=
includedir=

Name: libdiscid
Description: The MusicBrainz DiscID Library.
URL: http://musicbrainz.org/products/libdiscid/
Version: 0.6.4
Requires:
Libs: -ldiscid
Cflags:
EOF

########## cJSON ##########
wget https://github.com/DaveGamble/cJSON/archive/refs/tags/v1.7.16.tar.gz -O cJSON-1.7.16.tar.gz
rm -Rf cJSON-1.7.16
tar xfz cJSON-1.7.16.tar.gz
cd cJSON-1.7.16
patch -p 1 <<EOF
diff -u cJSON-1.7.15-orig/cJSON.c cJSON-1.7.15/cJSON.c
--- cJSON-1.7.15-orig/cJSON.c	2021-08-25 13:15:09.000000000 +0200
+++ cJSON-1.7.15/cJSON.c	2023-04-08 15:46:56.341804545 +0200
@@ -69,21 +69,7 @@
 #endif
 #define false ((cJSON_bool)0)
 
-/* define isnan and isinf for ANSI C, if in C99 or above, isnan and isinf has been defined in math.h */
-#ifndef isinf
-#define isinf(d) (isnan((d - d)) && !isnan(d))
-#endif
-#ifndef isnan
-#define isnan(d) (d != d)
-#endif
-
-#ifndef NAN
-#ifdef _WIN32
-#define NAN sqrt(-1.0)
-#else
-#define NAN 0.0/0.0
-#endif
-#endif
+#include <math.h>
 
 typedef struct {
     const unsigned char *json;
diff -u cJSON-1.7.15-orig/CMakeLists.txt cJSON-1.7.15/CMakeLists.txt
--- cJSON-1.7.15-orig/CMakeLists.txt	2021-08-25 13:15:09.000000000 +0200
+++ cJSON-1.7.15/CMakeLists.txt	2023-04-08 15:47:22.540175535 +0200
@@ -20,11 +20,9 @@
 if (ENABLE_CUSTOM_COMPILER_FLAGS)
     if (("\${CMAKE_C_COMPILER_ID}" STREQUAL "Clang") OR ("\${CMAKE_C_COMPILER_ID}" STREQUAL "GNU"))
         list(APPEND custom_compiler_flags
-            -std=c89
             -pedantic
             -Wall
             -Wextra
-            -Werror
             -Wstrict-prototypes
             -Wwrite-strings
             -Wshadow
EOF
do_cmake
cd ..

########## ancient ##########
wget https://github.com/temisu/ancient/archive/refs/tags/v2.1.1.tar.gz -O ancient-2.1.1.tar.gz
rm -Rf ancient-2.1.1
tar xfz ancient-2.1.1.tar.gz
cd ancient-2.1.1
mkdir -p m4
wget 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_cxx_compile_stdcxx.m4' -O m4/ax_cxx_compile_stdcxx.m4
wget 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_check_compile_flag.m4' -O m4/ax_check_compile_flag.m4
wget 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_cflags_warn_all.m4' -O m4/ax_cflags_warn_all.m4
wget 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_compiler_vendor.m4' -O m4/ax_compiler_vendor.m4
wget 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_prepend_flag.m4' -O m4/ax_prepend_flag.m4
wget 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_require_defined.m4' -O m4/ax_require_defined.m4
aclocal -I m4
autoreconf --install
./configure --host $host --prefix=$prefix
make all install
cd ..

########## libiconv ##########
wget https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz -O libiconv-1.17.tar.gz
rm -Rf libiconv-1.17
tar xfz libiconv-1.17.tar.gz
cd libiconv-1.17
./configure --host $host --prefix=$prefix
make all install
cd ..

########## game-music-emulator ##########
wget https://bitbucket.org/mpyne/game-music-emu/downloads/game-music-emu-0.6.3.tar.gz -O game-music-emu-0.6.3.tar.gz
rm -Rf game-music-emu-0.6.3
tar xfz game-music-emu-0.6.3.tar.gz
cd game-music-emu-0.6.3
do_cmake -DENABLE_UBSAN=off
cd ..

######### unifont ##########
wget https://unifoundry.com/pub/unifont/unifont-15.0.06/font-builds/unifont-15.0.06.otf       -O unifont-15.0.06.otf
wget https://unifoundry.com/pub/unifont/unifont-15.0.06/font-builds/unifont_csur-15.0.06.otf  -O unifont_csur-15.0.06.otf
wget https://unifoundry.com/pub/unifont/unifont-15.0.06/font-builds/unifont_upper-15.0.06.otf -O unifont_upper-15.0.06.otf

cd ..
./configure \
  --host $host \
  CFLAGS=-I$prefix/include/ \
  LDFLAGS=-L$prefix/lib/ \
  PKG_CONFIG_PATH=$prefix/lib/pkgconfig/ \
  --prefix="$install" \
  --libdir="$install" \
  --docdir="$install" \
  --bindir="$install" \
  --sysconfdir="$install" \
  --datadir="$install" \
  --datarootdir="$install" \
  --mandir="$install" \
  --without-info \
  --without-update-mime-database \
  --without-desktop_file_install \
  --without-update-desktop-database \
  --with-unifont-relative
make all
rm -Rf $install
make install
cp $prefix/lib/bz2.dll              \
   $prefix/lib/discid.dll           \
   $prefix/bin/libancient-2.dll     \
   $prefix/bin/libcharset-1.dll     \
   $prefix/bin/libcjson.dll         \
   $prefix/bin/libFLAC.dll          \
   $prefix/bin/libfreetype.dll      \
   $prefix/bin/libgme.dll           \
   $prefix/bin/libharfbuzz.dll      \
   $prefix/bin/libiconv-2.dll       \
   $prefix/bin/libjpeg-62.dll       \
   $prefix/bin/libmad-0.dll         \
   $prefix/bin/libpng16.dll         \
   $prefix/bin/libturbojpeg.dll     \
   $prefix/bin/ogg.dll              \
   $prefix/bin/SDL2.dll             \
   $prefix/bin/vorbis.dll           \
   $prefix/bin/vorbisfile.dll       \
   /usr/lib/gcc/$host/12-posix/libssp-0.dll       \
   /usr/lib/gcc/$host/12-posix/libstdc++-6.dll    \
   /usr/$host/lib/libwinpthread-1.dll             \
   /usr/$host/lib/zlib1.dll                       \
   $install
if test "$host" == "i686-w64-mingw32"; then
  cp $prefix/bin/libbrotlicommon.dll  \
     $prefix/bin/libbrotlidec.dll     \
     /usr/lib/gcc/$host/12-posix/libgcc_s_dw2-1.dll \
   $install
else
  cp /usr/lib/gcc/$host/12-posix/libgcc_s_seh-1.dll \
     $install
fi
mkdir -p $install/data
cp $host-src/unifont-15.0.06.otf       $install/data/unifont.otf
cp $host-src/unifont_csur-15.0.06.otf  $install/data/unifont_csur.otf
cp $host-src/unifont_upper-15.0.06.otf $install/data/unifont_upper.otf
$host-strip $install/*.dll $install/*.exe $install/autoload/*.dll
