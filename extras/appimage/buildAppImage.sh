#!/bin/bash

# AppImage BUILD SCRIPT FOR NAEV
#
# Written by Jack Greiner (ProjectSynchro on Github: https://github.com/ProjectSynchro/)
#
# For more information, see http://appimage.org/

buildpath=$MESON_BUILD_ROOT
linuxdeploy="$buildpath/linuxdeploy-x86_64.AppImage"
appdir=$DESTDIR

# Set ARCH of AppImage
#export ARCH=$(arch)

# Set App and VERSION variables
APP=Naev
export VERSION=$1
export OUTPUT="$MESON_BUILD_ROOT/naev.appimage"

# Get linuxdeploy's AppImage
if [ ! -f "$linuxdeploy" ]
then
    wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
        -O "$linuxdeploy"
    chmod +x "$linuxdeploy"
fi

# Run linuxdeploy and generate an AppDir, then generate an AppImage
"$linuxdeploy" \
    --appdir "$appdir" \
    --output appimage

# Move AppImage to dist/ and mark as executable
#chmod +x $OUTPUT
#mv $APP-$VERSION-$ARCH.AppImage dist/
