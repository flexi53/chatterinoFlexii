#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT
"""Macht die Logo-Varianten für Aussehen -> Stil -> Logo.

Vorlage ist das Symbol des Programms selbst (resources/chattiflexii.icns).
Es besteht aus drei Flächen: der weißen Kachel, dem grauen Körper des „C“
und dem farbigen Bogen darüber. Jede Variante sagt, was aus diesen dreien
wird - eine Farbe oder ein Muster.

    python3 scripts/make-app-icons.py

Schreibt nach resources/icons/chattiflexii-<name>.png.
"""

from __future__ import annotations

import math
import random
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image

WURZEL = Path(__file__).resolve().parent.parent
ZIEL = WURZEL / "resources" / "icons"
SEITE = 512

# Die drei Flächen der Vorlage
KACHEL = (255, 255, 255)
KOERPER = (152, 152, 152)
BOGEN = (225, 119, 255)


def vorlage() -> Image.Image:
    """Das Symbol des Programms als Bild, quadratisch und mit Alpha."""
    with tempfile.TemporaryDirectory() as ordner:
        satz = Path(ordner) / "logo.iconset"
        subprocess.run(
            ["iconutil", "--convert", "iconset",
             str(WURZEL / "resources" / "chattiflexii.icns"), "-o", str(satz)],
            check=True,
        )
        bild = Image.open(satz / "icon_512x512@2x.png").convert("RGBA")
    return bild.resize((SEITE, SEITE), Image.LANCZOS)


def gewichte(bild: Image.Image) -> tuple[np.ndarray, np.ndarray]:
    """Wie sehr jeder Punkt zu Kachel, Körper und Bogen gehört, dazu Alpha.

    Weich gerechnet, damit die Kanten so glatt bleiben wie in der Vorlage.
    """
    daten = np.asarray(bild).astype(np.float64)
    farben = daten[..., :3]
    alpha = daten[..., 3:4] / 255.0

    teile = []
    for farbe in (KACHEL, KOERPER, BOGEN):
        abstand = np.linalg.norm(farben - np.array(farbe, dtype=np.float64),
                                 axis=-1)
        teile.append(1.0 / np.power(abstand + 1.0, 4))
    roh = np.stack(teile, axis=-1)
    return roh / roh.sum(axis=-1, keepdims=True), alpha


def flaeche(farbe) -> np.ndarray:
    """Eine Fläche in einer Farbe."""
    return np.ones((SEITE, SEITE, 3), dtype=np.float64) * np.array(
        farbe, dtype=np.float64)


def verlauf(oben, unten, schraeg: bool = True) -> np.ndarray:
    """Ein Verlauf von @oben nach @unten, auf Wunsch über die Diagonale."""
    y, x = np.mgrid[0:SEITE, 0:SEITE]
    teil = ((x + y) / (2 * SEITE)) if schraeg else (y / SEITE)
    teil = teil[..., None]
    return (np.array(oben, dtype=np.float64) * (1 - teil) +
            np.array(unten, dtype=np.float64) * teil)


def streifen(farben) -> np.ndarray:
    """Waagerechte Streifen, wie eine Flagge."""
    bild = np.zeros((SEITE, SEITE, 3), dtype=np.float64)
    hoehe = SEITE / len(farben)
    for i, farbe in enumerate(farben):
        von, bis = int(i * hoehe), int((i + 1) * hoehe)
        bild[von:bis, :] = np.array(farbe, dtype=np.float64)
    return bild


def flecken(grund, farben, zahl: int = 26, streuung: int = 0) -> np.ndarray:
    """Weiche Flecken übereinander - für Tarnmuster."""
    zufall = random.Random(streuung)
    bild = flaeche(grund)
    y, x = np.mgrid[0:SEITE, 0:SEITE]
    for _ in range(zahl):
        mx = zufall.uniform(0, SEITE)
        my = zufall.uniform(0, SEITE)
        breite = zufall.uniform(SEITE * 0.045, SEITE * 0.13)
        hoehe = breite * zufall.uniform(0.5, 1.4)
        dreh = zufall.uniform(0, math.pi)
        fx = (x - mx) * math.cos(dreh) + (y - my) * math.sin(dreh)
        fy = -(x - mx) * math.sin(dreh) + (y - my) * math.cos(dreh)
        innen = ((fx / breite) ** 2 + (fy / hoehe) ** 2) <= 1.0
        farbe = np.array(zufall.choice(farben), dtype=np.float64)
        bild[innen] = farbe
    return bild


def sterne(grund, zahl: int = 90, streuung: int = 1) -> np.ndarray:
    """Ein Nachthimmel: dunkler Grund mit hellen Punkten."""
    zufall = random.Random(streuung)
    bild = flaeche(grund)
    y, x = np.mgrid[0:SEITE, 0:SEITE]
    for _ in range(zahl):
        mx = zufall.uniform(0, SEITE)
        my = zufall.uniform(0, SEITE)
        gross = zufall.uniform(2.0, 5.2)
        hell = zufall.uniform(0.6, 1.0)
        abstand = np.sqrt((x - mx) ** 2 + (y - my) ** 2)
        schein = np.clip(1.0 - (abstand / (gross * 2.2)), 0, 1) ** 2
        bild += schein[..., None] * (np.array([255.0, 255.0, 255.0]) - bild) * hell
    return np.clip(bild, 0, 255)


def variante(name: str, kachel, koerper, bogen, w, alpha) -> None:
    """Schreibt eine Variante; jede Fläche ist eine Farbe oder ein Muster."""
    flaechen = []
    for wunsch in (kachel, koerper, bogen):
        flaechen.append(flaeche(wunsch) if isinstance(wunsch, tuple) else wunsch)

    neu = sum(w[..., i:i + 1] * flaechen[i] for i in range(3))
    bild = np.concatenate([np.clip(neu, 0, 255), alpha * 255.0], axis=-1)
    Image.fromarray(bild.astype(np.uint8), "RGBA").save(
        ZIEL / f"chattiflexii-{name}.png")
    print(f"  {name}")


def main() -> int:
    ZIEL.mkdir(parents=True, exist_ok=True)
    w, alpha = gewichte(vorlage())
    print("Varianten:")

    # Schlicht: Kachel bleibt hell, Bogen und Körper wechseln zusammen
    variante("violett", KACHEL, KOERPER, BOGEN, w, alpha)
    variante("blau", KACHEL, (138, 152, 168), (91, 200, 255), w, alpha)
    variante("gruen", KACHEL, (140, 164, 148), (98, 224, 138), w, alpha)
    variante("orange", KACHEL, (168, 152, 136), (255, 162, 75), w, alpha)
    variante("rosa", KACHEL, (172, 146, 158), (255, 119, 180), w, alpha)

    # Ganz durchgefärbt, wie die Nitro-Symbole bei Discord
    variante("camouflage",
             (226, 222, 196),
             flecken((58, 68, 42), [(38, 46, 28), (86, 98, 56), (62, 54, 36),
                                    (112, 122, 74)], zahl=70, streuung=7),
             flecken((124, 142, 78), [(84, 102, 54), (168, 176, 112),
                                      (98, 84, 54), (196, 198, 148)],
                     zahl=60, streuung=3),
             w, alpha)
    variante("mitternacht",
             (10, 12, 26),
             sterne((34, 40, 82), zahl=70, streuung=9),
             sterne((62, 72, 142), zahl=150, streuung=5),
             w, alpha)
    variante("sonnenuntergang",
             (38, 22, 48),
             verlauf((122, 60, 110), (60, 34, 78)),
             verlauf((255, 176, 84), (255, 92, 152)),
             w, alpha)
    variante("neon",
             (14, 14, 22),
             (44, 44, 66),
             verlauf((80, 240, 255), (255, 80, 220)),
             w, alpha)
    variante("regenbogen",
             KACHEL,
             (150, 150, 150),
             streifen([(228, 60, 60), (240, 148, 52), (246, 220, 70),
                       (86, 200, 96), (72, 140, 236), (150, 92, 220)]),
             w, alpha)
    return 0


if __name__ == "__main__":
    sys.exit(main())
