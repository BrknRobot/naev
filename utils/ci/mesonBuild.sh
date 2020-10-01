#!/bin/bash
# Builds Naev via meson on CI Runners.
# TODO: Add addition al logging if build fails.
#
# This script should be run after dependencies are installed, as it depends on a working build system, and all build dependencies
#
# Checks if argument(s) are valid

set -e

while getopts d:r:j:o: OPTION "$@"; do
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
    o)
        OUTPUT=${OPTARG}
        ;;

    esac
done

if [[ -z "$RUNNER" ]]; then
    echo "usage: `basename $0` [-d] -r <runner.os> -j <JOB> -o <OUTPUT>"
    exit 2
elif [[ -z "$JOBNAME" ]]; then
    echo "usage: `basename $0` [-d] -r <runner.os> -j <JOB> -o <OUTPUT>"
    exit 2
elif [[ -z "$OUTPUT" ]]; then
    echo "usage: `basename $0` [-d] -r <runner.os> -j <JOB> -o <OUTPUT>"
    exit 2
fi

if [[ $RUNNER == 'Linux' ]]; then
    if [[ $JOBNAME == 'dist' ]]; then
        # Build dist source
        echo "Build dist source on $RUNNER"
        export PATH="/home/${USER}/.local/bin:$PATH"
        meson setup builddir -Dconfigure_build=false -Dconfigure_doc=false
        meson dist -C builddir --no-tests

    elif [[ $JOBNAME == 'ci' ]]; then
        # Build Naev
        echo "Build Naev on $RUNNER"
        export PATH="/home/${USER}/.local/bin:$PATH"
        meson setup build source \
            --buildtype release \
            --prefix="$OUTPUT/" \
            -Dconfigure_doc=false
        meson install -C build

    elif [[ $JOBNAME == 'docs' ]]; then
        # Build Naev Docs
        echo "Build Naev docs on $RUNNER"
        export PATH="/home/${USER}/.local/bin:$PATH"
        meson setup build source \
            --prefix="$OUTPUT/" \
            -Dconfigure_build=false
        meson install -C build

    else
        echo "Something went wrong figuring out what we are building.. Check the log.."
        exit -1
    fi
elif [[ $RUNNER == 'macOS' ]]; then
    if [[ $JOBNAME == 'dist' ]]; then
        # Build dist source
        echo "Build dist source on $RUNNER"
        export PATH="/Users/${USER}/.local/bin:$PATH"
        meson setup builddir -Dconfigure_build=false -Dconfigure_doc=false
        meson dist -C builddir --no-tests

    elif [[ $JOBNAME == 'ci' ]]; then
        # Build Naev
        echo "Build Naev on $RUNNER"

        # PKGCONFIG configuration for OpenAL (Shouldn't need this but meson can't seem to find the macOS bundled openal)

        export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/opt/openal-soft/lib/pkgconfig"
        export PATH="/Users/${USER}/.local/bin:$PATH"
        meson setup build source \
            --buildtype release \
            --prefix="$OUTPUT/naev.app" \
            --bindir=Contents/MacOS \
            --datadir=Contents/Resources \
            -Dconfigure_doc=false
        meson install -C build

    else
        echo "Something went wrong figuring out what we are building.. Check the log.."
        exit -1
    fi 
elif [[ $RUNNER == 'Windows' ]]; then/
    if [[ $JOBNAME == 'dist' ]]; then
        # Build dist source
        echo "Build dist source on $RUNNER"
        export PATH="/home/${USER}/.local/bin:$PATH"
        meson setup builddir -Dconfigure_build=false -Dconfigure_doc=false
        meson dist -C builddir --no-tests

    elif [[ $JOBNAME == 'ci' ]]; then
        # Build Naev
        echo "Build Naev on $RUNNER"
        export PATH="/home/${USER}/.local/bin:$PATH"
        meson setup build source \
            --buildtype release \
            --prefix="$OUTPUT/" \
            -Dconfigure_doc=false
        meson install -C build

    else
        echo "Something went wrong figuring out what we are building.. Check the log.."
        exit -1
    fi
else
    echo "Something went wrong building Naev.. Check the log.."
    exit -1
fi
