#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT
"""Zeichnet das Logo von ChattiFlexii: das „F“ aus Pinselstrichen.

Oben ein geschwungener lila Balken mit runden Enden, darunter der graue
Stamm mit dem Mittelbalken als Tropfen. Der Balken liegt über dem Stamm und
wirft einen leichten Schatten darauf.

Das Logo besteht aus genau den drei Flächen, die make-app-icons.py in jeder
Variante umfärbt - der weißen Kachel, dem grauen Körper und dem lila Bogen -
und benutzt deren Farben. Darum laufen alle Muster und Verläufe mit.

    python3 scripts/make-logo.py
    python3 scripts/make-app-icons.py

Schreibt resources/chattiflexii.icns (das Symbol des Programms und die
Vorlage der Varianten) sowie icon.png und icon.ico für Linux und Windows.
Die leere Kachel mit ihrem Schatten kommt aus resources/raw/logo-kachel.png.
"""

from __future__ import annotations

import math
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

WURZEL = Path(__file__).resolve().parent.parent
SEITE = 1024
# So viel feiner wird gezeichnet, damit die Kanten glatt werden
FEIN = 4

KOERPER = (152, 152, 152)
BOGEN = (225, 119, 255)

# Ein Pinselstrich ist eine Folge kubischer Kurven; jede schwillt von der
# ersten zur zweiten Dicke an (Halbmesser in Punkten von 1024)
BALKEN = [
    ((290, 330), (282, 240), (410, 220), (520, 248), 72, 68),
    ((520, 248), (610, 272), (680, 262), (734, 232), 68, 74),
]
STAMM = [((430, 275), (412, 430), (400, 600), (410, 770), 62, 88)]
MITTE = [((440, 575), (520, 585), (582, 575), (616, 545), 42, 74)]


def kurve(p0, p1, p2, p3, t: float) -> tuple[float, float]:
    u = 1 - t
    return (u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] +
            t**3 * p3[0],
            u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] +
            t**3 * p3[1])


def strich(teile) -> np.ndarray:
    """Wie sehr jeder Punkt zu einem Pinselstrich gehört, von 0 bis 1."""
    maske = Image.new("L", (SEITE * FEIN, SEITE * FEIN), 0)
    stift = ImageDraw.Draw(maske)
    for p0, p1, p2, p3, von, bis in teile:
        for i in range(601):
            t = i / 600
            x, y = kurve(p0, p1, p2, p3, t)
            # weich an- und abschwellen
            r = von + (bis - von) * (3 * t * t - 2 * t**3)
            stift.ellipse([(x - r) * FEIN, (y - r) * FEIN,
                           (x + r) * FEIN, (y + r) * FEIN], fill=255)
    klein = maske.resize((SEITE, SEITE), Image.LANCZOS)
    return np.asarray(klein, dtype=np.float64) / 255.0


def weich(maske: np.ndarray, radius: float, dy: int, dx: int) -> np.ndarray:
    """Ein verschobener, weicher Schatten von @a maske."""
    bild = Image.fromarray((maske * 255).astype(np.uint8))
    bild = bild.filter(ImageFilter.GaussianBlur(radius))
    return np.roll(np.asarray(bild, dtype=np.float64) / 255.0, (dy, dx),
                   (0, 1))


def logo() -> Image.Image:
    bogen = strich(BALKEN)
    koerper = np.maximum(strich(STAMM), strich(MITTE))
    marke = np.maximum(bogen, koerper)

    bild = np.asarray(
        Image.open(WURZEL / "resources" / "raw" / "logo-kachel.png")
        .convert("RGBA").resize((SEITE, SEITE), Image.LANCZOS),
        dtype=np.float64).copy()

    # Der Schatten des ganzen F auf der Kachel
    bild[..., :3] *= (1 - 0.16 * weich(marke, 14, 16, 6))[..., None]

    # Grau zuerst, mit dem Schatten des Balkens darauf
    grau = np.array(KOERPER, dtype=np.float64)[None, None, :] * (
        1 - 0.20 * weich(bogen, 12, 14, 4))[..., None]
    bild[..., :3] = (bild[..., :3] * (1 - koerper[..., None]) +
                     grau * koerper[..., None])

    # Lila darüber, oben links etwas heller
    y, x = np.mgrid[0:SEITE, 0:SEITE]
    licht = np.clip(1 - np.hypot(x - 260, y - 200) / 480, 0, 1)
    lila = (np.array(BOGEN, dtype=np.float64)[None, None, :] +
            licht[..., None] * np.array([8.0, 30.0, 0.0]))
    bild[..., :3] = (bild[..., :3] * (1 - bogen[..., None]) +
                     lila * bogen[..., None])

    return Image.fromarray(np.clip(bild, 0, 255).astype(np.uint8))


def freigestellt(bild: Image.Image) -> Image.Image:
    """Nur das F, ohne Kachel - für icon.png und icon.ico."""
    bogen = strich(BALKEN)
    koerper = np.maximum(strich(STAMM), strich(MITTE))
    marke = np.maximum(bogen, koerper)
    daten = np.asarray(bild, dtype=np.float64).copy()
    daten[..., 3] = marke * 255.0
    # Den Rand der Marke auf ihren Ausschnitt zuschneiden, quadratisch
    ys, xs = np.where(marke > 0.01)
    mitte = ((xs.min() + xs.max()) / 2, (ys.min() + ys.max()) / 2)
    halb = max(xs.max() - xs.min(), ys.max() - ys.min()) / 2 + 8
    kasten = tuple(int(round(v)) for v in (mitte[0] - halb, mitte[1] - halb,
                                           mitte[0] + halb, mitte[1] + halb))
    return Image.fromarray(daten.astype(np.uint8)).crop(kasten)


def icns(bild: Image.Image, ziel: Path) -> None:
    with tempfile.TemporaryDirectory() as ordner:
        satz = Path(ordner) / "logo.iconset"
        satz.mkdir()
        for groesse in (16, 32, 128, 256, 512):
            for faktor, endung in ((1, ""), (2, "@2x")):
                seite = groesse * faktor
                bild.resize((seite, seite), Image.LANCZOS).save(
                    satz / f"icon_{groesse}x{groesse}{endung}.png")
        subprocess.run(["iconutil", "--convert", "icns", str(satz), "-o",
                        str(ziel)], check=True)


def main() -> int:
    bild = logo()
    icns(bild, WURZEL / "resources" / "chattiflexii.icns")
    print("  chattiflexii.icns")

    frei = freigestellt(bild)
    frei.resize((256, 256), Image.LANCZOS).save(WURZEL / "resources" /
                                                "icon.png", optimize=True)
    print("  icon.png")
    frei.resize((256, 256), Image.LANCZOS).save(
        WURZEL / "resources" / "icon.ico",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128),
               (256, 256)])
    print("  icon.ico")
    return 0


if __name__ == "__main__":
    sys.exit(main())
