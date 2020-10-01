#!/bin/bash

# AppImage BUILD SCRIPT FOR NAEV
#
# Written by Jack Greiner (ProjectSynchro on Github: https://github.com/ProjectSynchro/) 
#
# For more information, see http://appimage.org/

# Set ARCH of AppImage
export ARCH=$(arch)

# Set App and VERSION variables
APP=Naev
VERSION=$(git rev-parse --short HEAD)

# Set DESTDIR for ninja
export DESTDIR=$(pwd)/dist/$APP.AppDir

# Build Naev with prefix /usr with no docs
meson setup build --buildtype release --prefix=/usr -Dconfigure_doc=false

# Build and install Naev to DESTDIR
ninja -C build
ninja -C build install

# Get linuxdeploy's AppImage
mkdir linuxdeploy-bin
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage
mv linuxdeploy-x86_64.AppImage linuxdeploy-bin/

# Run linuxdeploy and generate an AppDir, then generate an AppImage
./linuxdeploy-bin/linuxdeploy-x86_64.AppImage --appdir $(pwd)/dist/$APP.AppDir --output appimage

# Move AppImage to dist/ and mark as executable
chmod +x $APP-$VERSION-$ARCH.AppImage
mv $APP-$VERSION-$ARCH.AppImage dist/
