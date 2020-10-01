#!/bin/bash

# AppImage BUILD SCRIPT FOR NAEV
#
# Written by Jack Greiner (ProjectSynchro on Github: https://github.com/ProjectSynchro/) 
#
# For more information, see http://appimage.org/

export ARCH=$(arch)

APP=Naev
LOWERAPP=naev

GIT_REV=$(git rev-parse --short HEAD)
echo $GIT_REV

RELEASE_VERSION=$(git describe --tags)

export DESTDIR=$(pwd)/$APP.AppDir

meson setup build --buildtype release --prefix="/usr/" -Dconfigure_doc=false
ninja -C build install

wget -q https://github.com/probonopd/AppImages/raw/master/functions.sh -O ./functions.sh
#Remove line that should not be in functions.sh
sed -i -r 's/set -x//' functions.sh
#Silence wget
sed -i 's/wget/wget -q/' functions.sh
. ./functions.sh

cd $(pwd)/$APP.AppDir

########################################################################
# Copy desktop and icon file to AppDir for AppRun to pick them up
########################################################################

get_apprun
get_desktop
get_icon

########################################################################
# Copy in the dependencies that cannot be assumed to be available
# on all target systems
########################################################################

copy_deps

########################################################################
# Delete stuff that should not go into the AppImage
########################################################################

# Delete dangerous libraries; see
# https://github.com/probonopd/AppImages/blob/master/excludelist

delete_blacklisted

########################################################################
# Determine the version of the app; also include needed glibc version
########################################################################

VERSION=${RELEASE_VERSION}
export VERSION

########################################################################
# Patch away absolute paths
########################################################################

patch_usr

########################################################################
# AppDir complete
# Now packaging it as an AppImage
########################################################################

# Exit appimage
cd ..

mkdir -p out/
generate_type2_appimage
