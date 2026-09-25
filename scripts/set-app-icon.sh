#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT
#
# Tauscht das Symbol des installierten Programms gegen eine der Varianten
# aus resources/icons - das, was Finder, Dock und Launchpad zeigen, auch
# wenn ChattiFlexii nicht läuft. Das Symbol im laufenden Programm stellst
# du dagegen in den Einstellungen unter Aussehen -> Stil -> Logo.
#
#   scripts/set-app-icon.sh mitternacht [/Applications/ChattiFlexii.app]
#
# Danach wird neu signiert, damit die Signatur heil bleibt. Geht dabei
# etwas schief, kommt das alte Symbol zurück.

set -euo pipefail

VARIANTE="${1:-}"
APP="${2:-/Applications/ChattiFlexii.app}"

# Ohne Angabe: das, was in den Einstellungen steht - so bleibt das Symbol
# nach einer neuen Installation das, was zuletzt gewählt war
if [[ -z "$VARIANTE" ]]; then
    EINSTELLUNGEN="$HOME/Library/Application Support/ChattiFlexii/Settings/settings.json"
    if [[ -f "$EINSTELLUNGEN" ]]; then
        VARIANTE="$(python3 -c "
import json, sys
try:
    d = json.load(open(sys.argv[1]))
    print(d.get('appearance', {}).get('icon', 'violett'))
except Exception:
    print('violett')
" "$EINSTELLUNGEN")"
    fi
fi
IDENTITAET="${CHATTIFLEXII_SIGNIERUNG:-ChattiFlexii lokal}"

WURZEL="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QUELLE="$WURZEL/resources/icons/chattiflexii-$VARIANTE.png"

if [[ -z "$VARIANTE" || ! -f "$QUELLE" ]]; then
    echo "Welche Variante? Da ist:" >&2
    ls "$WURZEL/resources/icons" | sed 's/chattiflexii-//; s/\.png$//' | \
        sed 's/^/  /' >&2
    exit 1
fi

if [[ ! -d "$APP" ]]; then
    echo "Kein Programm unter $APP" >&2
    exit 1
fi

ZIEL="$APP/Contents/Resources/chattiflexii.icns"
if [[ ! -f "$ZIEL" ]]; then
    echo "In $APP steckt kein chattiflexii.icns" >&2
    exit 1
fi

ARBEIT="$(mktemp -d)"
trap 'rm -rf "$ARBEIT"' EXIT
SATZ="$ARBEIT/logo.iconset"
mkdir -p "$SATZ"

# Die Größen, die macOS erwartet - aus der Vorlage gerechnet
for paar in "16:icon_16x16" "32:icon_16x16@2x" "32:icon_32x32" \
            "64:icon_32x32@2x" "128:icon_128x128" "256:icon_128x128@2x" \
            "256:icon_256x256" "512:icon_256x256@2x" "512:icon_512x512" \
            "1024:icon_512x512@2x"; do
    groesse="${paar%%:*}"
    name="${paar##*:}"
    sips -z "$groesse" "$groesse" "$QUELLE" --out "$SATZ/$name.png" \
        >/dev/null 2>&1
done

iconutil --convert icns "$SATZ" -o "$ARBEIT/neu.icns"

# Das alte zur Seite, damit es zurückkann
cp "$ZIEL" "$ARBEIT/alt.icns"
cp "$ARBEIT/neu.icns" "$ZIEL"

zurueck() {
    cp "$ARBEIT/alt.icns" "$ZIEL"
    echo "Zurückgenommen - das alte Symbol steht wieder da." >&2
}

if security find-certificate -c "$IDENTITAET" >/dev/null 2>&1; then
    xattr -cr "$APP" || true
    if ! codesign -s "$IDENTITAET" --force --deep "$APP" >/dev/null 2>&1; then
        zurueck
        echo "Signieren ging nicht." >&2
        exit 1
    fi
    if ! codesign --verify --deep --strict "$APP" >/dev/null 2>&1; then
        zurueck
        codesign -s "$IDENTITAET" --force --deep "$APP" >/dev/null 2>&1 || true
        echo "Die Signatur hielt der Prüfung nicht stand." >&2
        exit 1
    fi
else
    echo "Kein Zertifikat „${IDENTITAET}“ - die Signatur des Programms" \
         "ist damit hinüber." >&2
    zurueck
    exit 1
fi

# Finder und Dock merken sich Symbole hartnäckig
touch "$APP"
killall Dock >/dev/null 2>&1 || true

echo "Das Symbol von ${APP} ist jetzt „${VARIANTE}“."
