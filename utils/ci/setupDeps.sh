#!/bin/bash
# Configures CI runners to install all build dependencies
# TODO: Add error detection if APT/Homebrew/PIP fails to update or install packages 
#
# This script should be run before building, as it installs the build system, and all build dependencies
#
# Checks if argument(s) are valid

set -e

while getopts d:r: OPTION "$@"; do
    case $OPTION in
    d)
        set -x
        ;;
    r)
        RUNNER=${OPTARG}
        ;;
    j)
        JOBNAME=${OPTARG}
        ;;     

    esac
done

if [[ -z "$RUNNER" ]]; then
    echo "usage: `basename $0` [-d] -r <runner.os>"
    exit 2
fi

if [[ $RUNNER == 'Linux' ]]; then
    # Update APT Cache
    echo "Update APT Cache"
    sudo apt-get update

    # Remove distro-packaged meson and ninja-build
    echo "Remove distro-packaged meson and ninja-build"
    sudo apt-get remove -y meson ninja-build

    # Install pipx dependencies
    echo "Install pipx dependencies"
    sudo apt-get install -y \
        python3-setuptools \
        python3-venv \

    # Install pipx
    echo "Install pipx"
    pip3 install pipx
    export PATH="/home/${USER}/.local/bin:$PATH"

    # Install meson and ninja via pipx
    echo "Install meson and ninja via pipx"
    pipx install meson
    pipx install ninja

    # Install APT packages for Build Dependencies
    echo "Install APT packages for Build Dependencies"
    if [[ $JOBNAME == 'docs' ]]; then
        sudo apt-get install -y \
            lua-ldoc \
            graphviz \
            doxygen
    elif [[ $JOBNAME == 'ci' ]]; then
        sudo apt-get install -y \
            build-essential \
            libsdl2-dev \
            libsdl2-mixer-dev \
            libsdl2-image-dev \
            libgl1-mesa-dev \
            libxml2-dev \
            libfontconfig1-dev \
            libfreetype6-dev \
            libpng-dev \
            libopenal-dev \
            libvorbis-dev \
            binutils-dev \
            libzip-dev \
            libiberty-dev \
            libluajit-5.1-dev \
            gettext \
            autopoint \
            intltool \
            lua-ldoc \
            graphviz \
            doxygen
    else
        echo "Nothing to install!"
    fi
elif [[ $RUNNER == 'macOS' ]]; then
    # Update APT Cache
    echo "Update Homebrew Cache"
    brew update

    # Install pipx
    echo "Install pipx"
    brew install pipx
    export PATH="/Users/${USER}/.local/bin:$PATH"

    # Install meson and ninja via pipx
    echo "Install meson and ninja via pipx"
    pipx install meson
    pipx install ninja

    # Install Homebrew Kegs for Build Dependencies
    echo "Install Homebrew Kegs for Build Dependencies"
        brew install \
          pkg-config \
          fontconfig \
          luajit \
          intltool \
          sdl2 \
          sdl2_mixer \
          sdl2_image \
          openal-soft

    # Remove Homebrew Perl (Causes conflicts with system perl)
    echo "Remove Homebrew Perl"
    brew uninstall --ignore-dependencies perl
        
elif [[ $RUNNER == 'Windows' ]]; then
    # Install pipx
    echo "Install pipx"
    pip3 install pipx
    export PATH="/home/${USER}/.local/bin:$PATH"

    # Install meson and ninja via pipx
    echo "Install meson and ninja via pipx"
    pipx install meson
    pipx install ninja

else
    echo "Something went wrong setting up dependencies.."
    exit -1
fi
