#!/bin/bash
# Check for latest version of external packages
#
# Known to work at Ubuntu 23.04
set -e

if ! test -f ./configure; then echo "Please execute from the main project directory like this:"; echo "mingw/versionscheck.sh"; exit 1; fi

. mingw/versionsconf.sh

BZIP2_LATEST_VERSION=`curl https://sourceware.org/bzip2/downloads.html -s | grep -m 1 "The current stable version is bzip2"|sed -e 's/.*The current stable version is bzip2 //'|sed -e 's/\.".*//' |sed -e 's/\.$//'`

if [ f"$BZIP2_VERSION" != f"$BZIP2_LATEST_VERSION" ]; then
	echo BZIP2 version should have been $BZIP2_LATEST_VERSION
fi

LIBMAD_LATEST_VERSION=`curl -s https://sourceforge.net/projects/mad/files/libmad/|grep -m 1 'span.*class="name"'|sed -e 's/.*=\"name\">//'|sed -e 's/<\/span.*//'`

if [ f"$LIBMAD_VERSION" != f"$LIBMAD_LATEST_VERSION" ]; then
	echo LIBMAD version should have been $LIBMAD_LATEST_VERSION
fi

LIBJPEGTURBO_LATEST_VERSION=`curl -s https://github.com/libjpeg-turbo/libjpeg-turbo/releases|grep -m 1 "Link--primary Link"|sed -e 's/.*"Link--primary Link">//'|sed -e 's/<.*//'`
if [ f"$LIBJPEGTURBO_VERSION" != f"$LIBJPEGTURBO_LATEST_VERSION" ]; then
	echo LIBJPEG-TURBO version should have been $LIBJPEGTURBO_LATEST_VERSION
fi

LIBPNG_LATEST_VERSION=`curl -s http://www.libpng.org/pub/png/libpng.html|grep -m 1 "The current public release"|sed -e 's/.*release,.*<B>libpng *//'|sed -e 's/<\/B.*//'`
if [ f"$LIBPNG_VERSION" != f"$LIBPNG_LATEST_VERSION" ]; then
	echo LIBPNG version should have been $LIBPNG_LATEST_VERSION
fi

LIBOGG_LATEST_VERSION=`curl -s https://xiph.org/downloads/|grep -m 1 libogg-|sed -e 's/.*libogg-//'|sed -e 's/\.tar\..*//'`
if [ f"$LIBOGG_VERSION" != f"$LIBOGG_LATEST_VERSION" ]; then
	echo LIBOGG version should have been $LIBOGG_LATEST_VERSION
fi

LIBVORBIS_LATEST_VERSION=`curl -s https://xiph.org/downloads/|grep -m 1 libvorbis-|sed -e 's/.*libvorbis-//'|sed -e 's/\.tar\..*//'`
if [ f"$LIBVORBIS_VERSION" != f"$LIBVORBIS_LATEST_VERSION" ]; then
	echo LIBVORBIS version should have been $LIBVORBIS_LATEST_VERSION
fi

FLAC_LATEST_VERSION=`curl -s https://xiph.org/downloads/|grep -m 1 flac-|sed -e 's/.*flac-//'|sed -e 's/\.tar\..*//'`
if [ f"$FLAC_VERSION" != f"$FLAC_LATEST_VERSION" ]; then
	echo FLAC version should have been $FLAC_LATEST_VERSION
fi

SDL2_LATEST_VERSION=`curl -s https://github.com/libsdl-org/SDL/releases|grep -m 1 "Link--primary Link.*>2\."|sed -e 's/.*"Link--primary Link">//'|sed -e 's/<.*//'`
if [ f"$SDL2_VERSION" != f"$SDL2_LATEST_VERSION" ]; then
	echo SDL2 version should have been $SDL2_LATEST_VERSION
fi

BROTLI_LATEST_VERSION=`curl -s https://github.com/google/brotli/releases|grep -m 1 "Link--primary Link.*>v[1-9]"|sed -e 's/.*"Link--primary Link">v//'|sed -e 's/<.*//'`
if [ f"$BROTLI_VERSION" != f"$BROTLI_LATEST_VERSION" ]; then
	echo BROTLI version should have been $BROTLI_LATEST_VERSION
fi

HARFBUZZ_LATEST_VERSION=`curl -s https://github.com/harfbuzz/harfbuzz/releases|grep -m 1 "Link--primary Link"|sed -e 's/.*"Link--primary Link">//'|sed -e 's/<.*//'`
if [ f"$HARFBUZZ_VERSION" != f"$HARFBUZZ_LATEST_VERSION" ]; then
	echo HARFBUZZ version should have been $HARFBUZZ_LATEST_VERSION
fi

FREETYPE2_LATEST_VERSION=`curl -s https://sourceforge.net/projects/freetype/files/freetype2/|grep -m 1 'span.*class="name"'|sed -e 's/.*=\"name\">//'|sed -e 's/<\/span.*//'`
if [ f"$FREETYPE2_VERSION" != f"$FREETYPE2_LATEST_VERSION" ]; then
	echo FREETYPE2 version should have been $FREETYPE2_LATEST_VERSION
fi

LIBDISCID_LATEST_VERSION=`curl -s https://github.com/metabrainz/libdiscid/releases|grep -m 1 "Link--primary Link.*>v[0-9]"|sed -e 's/.*"Link--primary Link">v//'|sed -e 's/<.*//'`
if [ f"$LIBDISCID_VERSION" != f"$LIBDISCID_LATEST_VERSION" ]; then
	echo LIBDISCID version should have been $LIBDISCID_LATEST_VERSION
fi

CJSON_LATEST_VERSION=`curl -s https://github.com/DaveGamble/cJSON/releases|grep -m 1 "Link--primary Link.*>v[1-9]"|sed -e 's/.*"Link--primary Link">v//'|sed -e 's/<.*//'`
if [ f"$CJSON_VERSION" != f"$CJSON_LATEST_VERSION" ]; then
	echo cJSON version should have been $CJSON_LATEST_VERSION
fi

ANCIENT_LATEST_VERSION=`curl -s https://github.com/temisu/ancient/releases|grep -m 1 "Link--primary Link.*>Ancient "|sed -e 's/.*"Link--primary Link">Ancient *//'|sed -e 's/<.*//'`
if [ f"$ANCIENT_VERSION" != f"$ANCIENT_LATEST_VERSION" ]; then
	echo ANCIENT version should have been $ANCIENT_LATEST_VERSION
fi

LIBICONV_LATEST_VERSION=`curl -s https://www.gnu.org/software/libiconv/|grep -m 1 'libiconv-.*\.tar\.gz'|sed -e 's/.*libiconv-//'|sed -e 's/\.tar\..*//'`
if [ f"$LIBICONV_VERSION" != f"$LIBICONV_LATEST_VERSION" ]; then
	echo LIBICONV version should have been $LIBICONV_LATEST_VERSION
fi

GAMEMUSICEMU_LATEST_VERSION=`curl -s https://github.com/libgme/game-music-emu/releases|grep -m 1 "Link--primary Link.*>game-music-emu-"|sed -e 's/.*"Link--primary Link">game-music-emu-//'|sed -e 's/<.*//'`
if [ f"$GAMEMUSICEMU_VERSION" != f"$GAMEMUSICEMU_LATEST_VERSION" ]; then
	echo Game Music Emu version should have been $GAMEMUSICEMU_LATEST_VERSION
fi

UNIFONT_LATEST_VERSION=`curl -s https://unifoundry.com/unifont/index.html|grep -m 1 "unifont-.*otf"|sed -e 's/.*unifont-//'|sed -e 's/\.otf.*//'`
if [ f"$UNIFONT_VERSION" != f"$UNIFONT_LATEST_VERSION" ]; then
	echo UNIFONT version should have been $UNIFONT_LATEST_VERSION
fi
