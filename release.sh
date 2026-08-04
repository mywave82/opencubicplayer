#!/usr/bin/env bash
set -e

VERSION=`cat configure|grep PACKAGE_VERSION=\'|head -n 1|sed -e 's/.*='\''//' -e 's/'\''//'`
SOURCE=`pwd`
cd ..
rm -Rf "ocp-$VERSION" "ocp-$VERSION.tar".* "ocp-$VERSION.7z" "ocp-windows-32bit-$VERSION.zip" "ocp-windows-32bit-$VERSION.zip"
git clone "$SOURCE" "ocp-$VERSION"
cd "ocp-$VERSION"
git submodule update --init --recursive
rm .git -Rf
rm -Rf "`find|grep .gitignore`"
rm -Rf "`find|grep .gitmodules`"
rm -Rf "`find -type d|grep .github`"
cd ..
tar c "ocp-$VERSION" | gzip -9 -n > "ocp-$VERSION".tar.gz
tar c "ocp-$VERSION" | bzip2 -9 > "ocp-$VERSION".tar.bz2
tar c "ocp-$VERSION" | xz -9 > "ocp-$VERSION".tar.xz
tar c "ocp-$VERSION" | lzma -z -9 > "ocp-$VERSION".tar.lzma
7z a -mx9 "ocp-$VERSION".7z "ocp-$VERSION"
cd "ocp-$VERSION"
mingw/build.sh i686-w64-mingw32
mingw/build.sh x86_64-w64-mingw32
cd i686-w64-mingw32-install
zip -r -9 "../../ocp-windows-32bit-$VERSION.zip" .
cd ../x86_64-w64-mingw32-install
zip -r -9 "../../ocp-windows-64bit-$VERSION.zip" .
cd ..
