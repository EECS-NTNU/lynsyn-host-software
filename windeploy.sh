#!/bin/bash

windeployqt bin/lynsyn_viewer.exe
windeployqt bin/lynsyn_xvc.exe
cp /c/msys64/mingw64/bin/libbz2-1.dll bin/
cp /c/msys64/mingw64/bin/libfreetype-6.dll bin/
cp /c/msys64/mingw64/bin/libgcc_s_seh-1.dll bin/
cp /c/msys64/mingw64/bin/libglib-2.0-0.dll bin/
cp /c/msys64/mingw64/bin/libgraphite2.dll bin/
cp /c/msys64/mingw64/bin/libharfbuzz-0.dll bin/
cp /c/msys64/mingw64/bin/libiconv-2.dll bin/
cp /c/msys64/mingw64/bin/libintl-8.dll bin/
cp /c/msys64/mingw64/bin/libpcre-1.dll bin/
cp /c/msys64/mingw64/bin/libpcre2-16-0.dll bin/
cp /c/msys64/mingw64/bin/libpng16-16.dll bin/
cp /c/msys64/mingw64/bin/libstdc++-6.dll bin/
cp /c/msys64/mingw64/bin/libusb-1.0.dll bin/
cp /c/msys64/mingw64/bin/libwinpthread-1.dll bin/
cp /c/msys64/mingw64/bin/libzstd.dll bin/
cp /c/msys64/mingw64/bin/zlib1.dll bin/
cp /c/msys64/mingw64/bin/libicuin64.dll bin/
cp /c/msys64/mingw64/bin/libicuuc64.dll bin/
cp /c/msys64/mingw64/bin/libicudt64.dll bin/
cp /mingw64/bin/addr2line bin/
cp /mingw64/bin/nm bin/
cp jtagdevices bin/jtagdevices
cp ../zadig-2.4.exe bin/
cp COPYING bin/COPYING.txt
cp -r fwbin bin/
