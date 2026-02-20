#!/bin/bash
# Cross compile for windows from Linux:
#
# Known to work at Ubuntu 23.04
set -e

if ! test -f ./configure || test $# != 1; then echo "Please execute from the main project directory like this:"; echo "mingw/build.sh x86_64-w64-mingw32    (or i686-w64-mingw32 for a 32bit build)"; exit 1; fi
if test "$1" != "x86_64-w64-mingw32" && test "$1" != "i686-w64-mingw32"; then echo "only supported targets are x86_64-w64-mingw32 and i686-w64-mingw32. $1 given"; exit 1; fi

. mingw/versionsconf.sh

test -f ./Makefile && make clean || true

sudo apt-get install mingw-w64-tools mingw-w64 libz-mingw-w64-dev nasm cmake

host=$1
if test "$host" == "i686-w64-mingw32"; then
 sudo update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix
else
 sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
fi

install=`pwd`/$host-install
prefix=`pwd`/$host-prefix
gccversion=`$host-gcc --version|head -n 1|sed -e 's/.*\ //'`
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

WGET="wget --tries=20 --waitretry=5 --retry-connrefused"

#$WGET 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_cxx_compile_stdcxx.m4' -O ax_cxx_compile_stdcxx.m4
test -f ax_cxx_compile_stdcxx.m4 || ( sleep 5 && $WGET 'https://gitweb.git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_cxx_compile_stdcxx.m4' -O ax_cxx_compile_stdcxx.m4 )
#WGET 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_check_compile_flag.m4' -O ax_check_compile_flag.m4
test -f ax_check_compile_flag.m4 || ( sleep 5 && $WGET 'https://gitweb.git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_check_compile_flag.m4' -O ax_check_compile_flag.m4 )
#$WGET 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_cflags_warn_all.m4' -O ax_cflags_warn_all.m4
test -f ax_cflags_warn_all.m4    || ( sleep 5 && $WGET 'https://gitweb.git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_cflags_warn_all.m4' -O ax_cflags_warn_all.m4 )
#$WGET 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_compiler_vendor.m4' -O ax_compiler_vendor.m4
test -f ax_compiler_vendor.m4    || ( sleep 5 && $WGET 'https://gitweb.git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_compiler_vendor.m4' -O ax_compiler_vendor.m4 )
#$WGET 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_prepend_flag.m4' -O ax_prepend_flag.m4
test -f ax_prepend_flag.m4       || ( sleep 5 && $WGET 'https://gitweb.git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_prepend_flag.m4' -O ax_prepend_flag.m4 )
#$WGET 'http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_require_defined.m4' -O ax_require_defined.m4
test -f ax_require_defined.m4    || ( sleep 5 && $WGET 'https://gitweb.git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_require_defined.m4' -O ax_require_defined.m4 )

test -f bzip2-$BZIP2_VERSION.tar.gz                 || $WGET https://sourceware.org/pub/bzip2/bzip2-$BZIP2_VERSION.tar.gz -O bzip2-$BZIP2_VERSION.tar.gz                                                                                     || rm bzip2-$BZIP2_VERSION.tar.gz                 || false
test -f libmad-$LIBMAD_VERSION.tar.gz               || $WGET https://sourceforge.net/projects/mad/files/libmad/$LIBMAD_VERSION/libmad-$LIBMAD_VERSION.tar.gz/download -O libmad-$LIBMAD_VERSION.tar.gz                                       || rm libmad-$LIBMAD_VERSION.tar.gz               || false
test -f libjpeg-turbo-$LIBJPEGTURBO_VERSION.tar.gz  || $WGET https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$LIBJPEGTURBO_VERSION/libjpeg-turbo-$LIBJPEGTURBO_VERSION.tar.gz -O libjpeg-turbo-$LIBJPEGTURBO_VERSION.tar.gz || rm libjpeg-turbo-$LIBJPEGTURBO_VERSION.tar.gz  || false
test -f libpng-$LIBPNG_VERSION.tar.gz               || $WGET https://download.sourceforge.net/libpng/libpng-$LIBPNG_VERSION.tar.gz -O libpng-$LIBPNG_VERSION.tar.gz                                                                          || rm libpng-$LIBPNG_VERSION.tar.gz               || false
test -f libogg-$LIBOGG_VERSION.tar.gz               || $WGET https://downloads.xiph.org/releases/ogg/libogg-$LIBOGG_VERSION.tar.gz -O libogg-$LIBOGG_VERSION.tar.gz                                                                          || rm libogg-$LIBOGG_VERSION.tar.gz               || false
test -f libvorbis-$LIBVORBIS_VERSION.tar.gz         || $WGET https://downloads.xiph.org/releases/vorbis/libvorbis-$LIBVORBIS_VERSION.tar.gz -O libvorbis-$LIBVORBIS_VERSION.tar.gz                                                           || rm libvorbis-$LIBVORBIS_VERSION.tar.gz         || false
test -f flac-$FLAC_VERSION.tar.xz                   || $WGET https://ftp.osuosl.org/pub/xiph/releases/flac/flac-$FLAC_VERSION.tar.xz -O flac-$FLAC_VERSION.tar.xz                                                                            || rm flac-$FLAC_VERSION.tar.xz                   || false
test -f SDL2-devel-$SDL2_VERSION-mingw.tar.gz       || $WGET https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VERSION/SDL2-devel-$SDL2_VERSION-mingw.tar.gz -O SDL2-devel-$SDL2_VERSION-mingw.tar.gz                        || rm SDL2-devel-$SDL2_VERSION-mingw.tar.gz       || false
if test "$host" == "i686-w64-mingw32"; then
  test -f brotli-$BROTLI_VERSION.tar.gz             || $WGET https://github.com/google/brotli/archive/refs/tags/v$BROTLI_VERSION.tar.gz -O brotli-$BROTLI_VERSION.tar.gz                                                                     || rm brotli-$BROTLI_VERSION.tar.gz               || false
fi
test -f harfbuzz-$HARFBUZZ_VERSION.tar.xz           || $WGET https://github.com/harfbuzz/harfbuzz/releases/download/$HARFBUZZ_VERSION/harfbuzz-$HARFBUZZ_VERSION.tar.xz -O harfbuzz-$HARFBUZZ_VERSION.tar.xz                                 || rm harfbuzz-$HARFBUZZ_VERSION.tar.xz           || false
test -f freetype-$FREETYPE2_VERSION.tar.gz          || $WGET https://sourceforge.net/projects/freetype/files/freetype2/$FREETYPE2_VERSION/freetype-$FREETYPE2_VERSION.tar.gz/download -O freetype-$FREETYPE2_VERSION.tar.gz                  || rm freetype-$FREETYPE2_VERSION.tar.gz          || false
#$WGET http://ftp.eu.metabrainz.org/pub/musicbrainz/libdiscid/libdiscid-$LIBDISCID_VERSION-win.zip -O libdiscid-$LIBDISCID_VERSION-win.zip
test -f libdiscid-$LIBDISCID_VERSION-win.zip        || $WGET https://github.com/metabrainz/libdiscid/releases/download/v$LIBDISCID_VERSION/libdiscid-$LIBDISCID_VERSION-win.zip -O libdiscid-$LIBDISCID_VERSION-win.zip                      || rm libdiscid-$LIBDISCID_VERSION-win.zip        || false
test -f cJSON-$CJSON_VERSION.tar.gz                 || $WGET https://github.com/DaveGamble/cJSON/archive/refs/tags/v$CJSON_VERSION.tar.gz -O cJSON-$CJSON_VERSION.tar.gz                                                                     || rm cJSON-$CJSON_VERSION.tar.gz                 || false
test -f ancient-$ANCIENT_VERSION.tar.gz             || $WGET https://github.com/temisu/ancient/archive/refs/tags/v$ANCIENT_VERSION.tar.gz -O ancient-$ANCIENT_VERSION.tar.gz                                                                 || rm ancient-$ANCIENT_VERSION.tar.gz             || false
test -f libiconv-$LIBICONV_VERSION.tar.gz           || $WGET https://ftp.gnu.org/pub/gnu/libiconv/libiconv-$LIBICONV_VERSION.tar.gz -O libiconv-$LIBICONV_VERSION.tar.gz                                                                     || rm libiconv-$LIBICONV_VERSION.tar.gz           || false
test -f game-music-emu-$GAMEMUSICEMU_VERSION.tar.gz || $WGET https://github.com/libgme/game-music-emu/archive/refs/tags/$GAMEMUSICEMU_VERSION.tar.gz -O game-music-emu-$GAMEMUSICEMU_VERSION.tar.gz                                 || rm game-music-emu-$GAMEMUSICEMU_VERSION.tar.gz || false
# Future releases will be here https://github.com/libgme/game-music-emu/tags
test -f unifont-$UNIFONT_VERSION.otf                || $WGET https://unifoundry.com/pub/unifont/unifont-$UNIFONT_VERSION/font-builds/unifont-$UNIFONT_VERSION.otf       -O unifont-$UNIFONT_VERSION.otf                                      || rm unifont-$UNIFONT_VERSION.otf                || false
test -f unifont_csur-$UNIFONT_VERSION.otf           || $WGET https://unifoundry.com/pub/unifont/unifont-$UNIFONT_VERSION/font-builds/unifont_csur-$UNIFONT_VERSION.otf  -O unifont_csur-$UNIFONT_VERSION.otf                                 || rm unifont_csur-$UNIFONT_VERSION.otf           || false
test -f unifont_upper-$UNIFONT_VERSION.otf          || $WGET https://unifoundry.com/pub/unifont/unifont-$UNIFONT_VERSION/font-builds/unifont_upper-$UNIFONT_VERSION.otf -O unifont_upper-$UNIFONT_VERSION.otf                                || rmunifont_upper-$UNIFONT_VERSION.otf           || false

#wget https://zlib.net/zlib-1.2.13.tar.gz
#tar xfz zlib-1.2.13.tar.gz
#cd zlib-1.2.13/
#prefix=$prefix CC=$host-gcc CFLAGS="-O2" ./configure
#make
#cd ..

########## BZIP2 ##########
dirs=`ls -d */|cut -f1|grep 'bzip2-*'` && rm -Rf $dirs
tar xfz bzip2-$BZIP2_VERSION.tar.gz
cd bzip2-$BZIP2_VERSION
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
dirs=`ls -d */|cut -f1|grep 'libmad-*'` && rm -Rf $dirs
tar xfz libmad-$LIBMAD_VERSION.tar.gz
cd libmad-$LIBMAD_VERSION
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
dirs=`ls -d */|cut -f1|grep 'libjpeg-turbo-*'` && rm -Rf $dirs
tar xfz libjpeg-turbo-$LIBJPEGTURBO_VERSION.tar.gz
cd libjpeg-turbo-$LIBJPEGTURBO_VERSION
do_cmake -DENABLE_SHARED=TRUE
cd ..

########## LIBPNG ##########
dirs=`ls -d */|cut -f1|grep 'libpng-*'` && rm -Rf $dirs
tar xfz libpng-$LIBPNG_VERSION.tar.gz
cd libpng-$LIBPNG_VERSION
#cmake does not use pkg-config for zlib?????
do_cmake -DZLIB_ROOT=/usr/$host/
cd ..

########## LIBOGG ##########
dirs=`ls -d */|cut -f1|grep 'libogg-*'` && rm -Rf $dirs
tar xfz libogg-$LIBOGG_VERSION.tar.gz
cd libogg-$LIBOGG_VERSION
do_cmake
cd ..
# Bug in build-system, missmatch with dll filename, and internal name
mv -f $prefix/bin/libogg.dll $prefix/bin/ogg.dll

########## VORBIS ##########
dirs=`ls -d */|cut -f1|grep 'libvorbis-*'` && rm -Rf $dirs
tar xfz libvorbis-$LIBVORBIS_VERSION.tar.gz
cd libvorbis-$LIBVORBIS_VERSION
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
dirs=`ls -d */|cut -f1|grep 'flac-*'` && rm -Rf $dirs
tar xfJ flac-$FLAC_VERSION.tar.xz
cd flac-$FLAC_VERSION
do_cmake -D_OGG_LIBRARY_DIRS=$prefix/lib
cd ..

########## SDL2 ##########
dirs=`ls -d */|cut -f1|grep 'SDL2-*'` && rm -Rf $dirs
tar xfz SDL2-devel-$SDL2_VERSION-mingw.tar.gz
cd SDL2-$SDL2_VERSION
cp $host/* $prefix -R
prefix2=`echo $prefix|sed -e 's/\//\\\\\//'g`
sed -e "s/\/tmp\/tardir\/.*\/build-mingw\/.*mingw..\//$prefix2\//" -i $prefix/bin/sdl2-config
sed -e "s/\/tmp\/tardir\/.*\/build-mingw\/.*mingw..\//$prefix2\//" -i $prefix/lib/libSDL2.la
sed -e "s/\/tmp\/tardir\/.*\/build-mingw\/.*mingw..\//$prefix2\//" -i $prefix/lib/libSDL2main.la
sed -e "s/\/tmp\/tardir\/.*\/build-mingw\/.*mingw..\//$prefix2\//" -i $prefix/lib/libSDL2_test.la
sed -e "s/\/tmp\/tardir\/.*\/build-mingw\/.*mingw../$prefix2\//" -i $prefix/lib/pkgconfig/sdl2.pc
sed -e "s/\/tmp\/tardir\/.*\/build-mingw\/.*mingw..\//$prefix2\//" -i $prefix/lib/cmake/SDL2/sdl2-config.cmake
cd ..

########## BROTLI ##########, only needed for 32bit, unsure why
dirs=`ls -d */|cut -f1|grep 'brotli-*'` && rm -Rf $dirs
if test "$host" == "i686-w64-mingw32"; then
  $WGET https://github.com/google/brotli/archive/refs/tags/v$BROTLI_VERSION.tar.gz -O brotli-$BROTLI_VERSION.tar.gz
  tar xfz brotli-$BROTLI_VERSION.tar.gz
  cd brotli-$BROTLI_VERSION
  do_cmake -DCMAKE_CXX_FLAGS=-Wa,-mbig-obj
  cd ..
fi

########## HARFBUZZ #########
dirs=`ls -d */|cut -f1|grep 'harfbuzz-*'` && rm -Rf $dirs
tar xfJ harfbuzz-$HARFBUZZ_VERSION.tar.xz
cd harfbuzz-$HARFBUZZ_VERSION
do_cmake -DCMAKE_CXX_FLAGS=-Wa,-mbig-obj
cd ..

########## FREETYPE2 ##########
dirs=`ls -d */|cut -f1|grep 'freetype-*'` && rm -Rf $dirs
tar xfz freetype-$FREETYPE2_VERSION.tar.gz
cd freetype-$FREETYPE2_VERSION
do_cmake -DZLIB_ROOT=/usr/$host/ \
         -DFT_DISABLE_BZIP2=TRUE
cd ..

########## LIBDISCID ##########
dirs=`ls -d */|cut -f1|grep 'libdiscid-*'` && rm -Rf $dirs
unzip libdiscid-$LIBDISCID_VERSION-win.zip
if test "$host" == "i686-w64-mingw32"; then
  cp -r libdiscid-$LIBDISCID_VERSION-win/Win32/* $prefix/lib
else
  cp -r libdiscid-$LIBDISCID_VERSION-win/x64/* $prefix/lib
fi
cp -r libdiscid-$LIBDISCID_VERSION-win/include/discid $prefix/include
cat <<EOF > $prefix/lib/pkgconfig/libdiscid.pc
prefix=
exec_prefix=
libdir=
includedir=

Name: libdiscid
Description: The MusicBrainz DiscID Library.
URL: http://musicbrainz.org/products/libdiscid/
Version: 0.6.5
Requires:
Libs: -ldiscid
Cflags:
EOF

########## cJSON ##########
dirs=`ls -d */|cut -f1|grep 'cJSON-*'` && rm -Rf $dirs
tar xfz cJSON-$CJSON_VERSION.tar.gz
cd cJSON-$CJSON_VERSION
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
dirs=`ls -d */|cut -f1|grep 'ancient-*'` && rm -Rf $dirs
tar xfz ancient-$ANCIENT_VERSION.tar.gz
cd ancient-$ANCIENT_VERSION
mkdir -p m4
cp ../ax_cxx_compile_stdcxx.m4 m4/
cp ../ax_cxx_compile_stdcxx.m4 m4/
cp ../ax_check_compile_flag.m4 m4/
cp ../ax_cflags_warn_all.m4    m4/
cp ../ax_compiler_vendor.m4    m4/
cp ../ax_prepend_flag.m4       m4/
cp ../ax_require_defined.m4    m4/
aclocal -I m4
autoreconf --install
./configure --host $host --prefix=$prefix
make all install
cd ..

########## libiconv ##########
dirs=`ls -d */|cut -f1|grep 'libiconv-*'` && rm -Rf $dirs
tar xfz libiconv-$LIBICONV_VERSION.tar.gz
cd libiconv-$LIBICONV_VERSION
./configure --host $host --prefix=$prefix
make all install
cd ..

########## game-music-emulator ##########
dirs=`ls -d */|cut -f1|grep 'game-music-emu-*'` && rm -Rf $dirs
tar xfz game-music-emu-$GAMEMUSICEMU_VERSION.tar.gz
cd game-music-emu-$GAMEMUSICEMU_VERSION
do_cmake -DENABLE_UBSAN=off
cd ..

######### unifont ##########

cd ..
./configure \
  --host $host \
  CFLAGS="-I$prefix/include/ -O2" \
  CXXFLAGS="-I$prefix/include/ -O2" \
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
   /usr/lib/gcc/$host/$gccversion/libssp-0.dll       \
   /usr/lib/gcc/$host/$gccversion/libstdc++-6.dll    \
   /usr/$host/lib/libwinpthread-1.dll             \
   /usr/$host/lib/zlib1.dll                       \
   $install
if test "$host" == "i686-w64-mingw32"; then
  cp $prefix/bin/libbrotlicommon.dll  \
     $prefix/bin/libbrotlidec.dll     \
     /usr/lib/gcc/$host/$gccversion/libgcc_s_dw2-1.dll \
   $install
else
  cp /usr/lib/gcc/$host/$gccversion/libgcc_s_seh-1.dll \
     $install
fi
mkdir -p $install/data
cp $host-src/unifont-$UNIFONT_VERSION.otf       $install/data/unifont.otf
cp $host-src/unifont_csur-$UNIFONT_VERSION.otf  $install/data/unifont_csur.otf
cp $host-src/unifont_upper-$UNIFONT_VERSION.otf $install/data/unifont_upper.otf
$host-strip $install/*.dll $install/*.exe $install/autoload/*.dll
wget https://stian.cubic.org/mirror/ftp.cubic.org/pub/player/gfx/opencp25image1.zip -O $install/data/opencp25image1.zip
wget https://stian.cubic.org/mirror/ftp.cubic.org/pub/player/gfx/opencp25ani1.zip -O $install/data/opencp25ani1.zip
