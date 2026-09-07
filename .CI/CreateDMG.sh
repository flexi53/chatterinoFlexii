#!/usr/bin/env bash

set -eo pipefail

# Overridable so forks that rename the application can reuse this script
APP_NAME="${APP_NAME:-chatterino}"
APP_BUNDLE="${APP_NAME}.app"
DMG_VOLUME_NAME="${DMG_VOLUME_NAME:-Chatterino2}"

if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: No '$APP_BUNDLE' dir found in the build directory. Make sure you've run ./CI/MacDeploy.sh"
    exit 1
fi

if [ -z "$OUTPUT_DMG_PATH" ]; then
    echo "ERROR: Must specify the path for where to save the final .dmg. Make sure you've set the OUTPUT_DMG_PATH environment variable."
    exit 1
fi

if [ -z "$SKIP_VENV" ]; then
    echo "Creating python3 virtual environment"
    python3 -m venv venv
    echo "Entering python3 virtual environment"
    . venv/bin/activate
    echo "Installing dmgbuild"
    python3 -m pip install dmgbuild
fi

if [ -n "$MACOS_CODESIGN_CERTIFICATE" ]; then
    echo "Codesigning force deep inside the app"
    codesign -s "$MACOS_CODESIGN_CERTIFICATE" --deep --force "$APP_BUNDLE"
    echo "Done!"
fi

echo "Running dmgbuild.."
dmgbuild --settings ./../.CI/dmg-settings.py -D app="./$APP_BUNDLE" "$DMG_VOLUME_NAME" "$OUTPUT_DMG_PATH"
echo "Done!"

if [ -n "$MACOS_CODESIGN_CERTIFICATE" ]; then
    echo "Codesigning the dmg"
    codesign -s "$MACOS_CODESIGN_CERTIFICATE" --deep --force "$OUTPUT_DMG_PATH"
    echo "Done!"
fi
