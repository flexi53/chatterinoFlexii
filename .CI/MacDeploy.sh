#!/usr/bin/env bash

# Bundle relevant qt & system dependencies into the ./<app>.app folder
#
# The bundle name can be overridden with APP_NAME so forks that rename the
# application can reuse this script unchanged.

set -eo pipefail

APP_NAME="${APP_NAME:-chatterino}"
APP_BUNDLE="${APP_NAME}.app"

if [ -d "bin/$APP_BUNDLE" ] && [ ! -d "$APP_BUNDLE" ]; then
    >&2 echo "Moving bin/$APP_BUNDLE down one directory"
    mv "bin/$APP_BUNDLE" "$APP_BUNDLE"
fi

if [ -n "$Qt5_DIR" ]; then
    echo "Using Qt DIR from Qt5_DIR: $Qt5_DIR"
    _QT_DIR="$Qt5_DIR"
    _img_version="5.15.2"
elif [ -n "$Qt6_DIR" ]; then
    echo "Using Qt DIR from Qt6_DIR: $Qt6_DIR"
    _QT_DIR="$Qt6_DIR"
    _img_version="6.9.3"
fi

if [ -n "$_QT_DIR" ]; then
    export PATH="${_QT_DIR}/bin:$PATH"
else
    echo "No Qt environment variable set, assuming system-installed Qt"
fi

echo "Running MACDEPLOYQT"

_macdeployqt_args=()

if [ -n "$MACOS_CODESIGN_CERTIFICATE" ]; then
    _macdeployqt_args+=("-codesign=$MACOS_CODESIGN_CERTIFICATE")
fi

echo "Extracting kimageformats plugins"
7z e -okimg kimg.zip

if [ -n "$MACOS_CODESIGN_CERTIFICATE" ]; then
    echo "Codesigning libKF6Archive"
    codesign -s "$MACOS_CODESIGN_CERTIFICATE" --force kimg/libKF6Archive.6.dylib
    echo "Codesigning kimg_avif"
    codesign -s "$MACOS_CODESIGN_CERTIFICATE" --force kimg/kimg_avif.dylib
fi

mkdir -p "$APP_BUNDLE/Contents/Frameworks"
mkdir -p "$APP_BUNDLE/Contents/PlugIns/imageformats"
cp kimg/libKF6Archive.6.dylib "$APP_BUNDLE/Contents/Frameworks/"
cp kimg/kimg_avif.dylib "$APP_BUNDLE/Contents/PlugIns/imageformats/"

macdeployqt "$APP_BUNDLE" "${_macdeployqt_args[@]}" -verbose=1

if [ -n "$MACOS_CODESIGN_CERTIFICATE" ]; then
    # Validate that the bundle was codesigned correctly
    codesign -v "$APP_BUNDLE"
fi
