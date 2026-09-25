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


def verlauf_mehr(farben, schraeg: bool = True, von: float = 0.0,
                 bis: float = 1.0) -> np.ndarray:
    """Ein Verlauf über mehrere Farben, ohne harte Kanten.

    @a von und @a bis sagen, über welchen Teil des Bildes er läuft - so
    liegen alle Farben in der Fläche, die sie tragen soll, statt zur Hälfte
    daneben.
    """
    y, x = np.mgrid[0:SEITE, 0:SEITE]
    teil = ((x + y) / (2 * SEITE)) if schraeg else (y / SEITE)
    teil = np.clip((teil - von) / max(bis - von, 1e-6), 0.0, 1.0)
    stellen = np.linspace(0.0, 1.0, len(farben))
    kanal = []
    for i in range(3):
        werte = [float(f[i]) for f in farben]
        kanal.append(np.interp(teil, stellen, werte))
    return np.stack(kanal, axis=-1)


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


def bereich(w, teil: int) -> tuple[float, float]:
    """Von wo bis wo die Fläche @a teil senkrecht reicht, als Anteil."""
    zeilen = np.nonzero((w[..., teil] > 0.5).any(axis=1))[0]
    if zeilen.size == 0:
        return 0.0, 1.0
    return float(zeilen.min()) / SEITE, float(zeilen.max()) / SEITE


def variante(name: str, koerper, bogen, w, alpha) -> None:
    """Schreibt eine Variante - nur die Marke, ohne die Kachel dahinter.

    Die weiße Fläche der Vorlage wird durchsichtig; was von ihr übrig
    bleibt, ist der weiche Rand der Marke.
    """
    flaechen = []
    for wunsch in (koerper, bogen):
        flaechen.append(flaeche(wunsch) if isinstance(wunsch, tuple) else wunsch)

    marke = w[..., 1:2] + w[..., 2:3]
    anteil = np.clip(marke, 1e-6, None)
    neu = (w[..., 1:2] * flaechen[0] + w[..., 2:3] * flaechen[1]) / anteil
    # Was zur Kachel gehörte, verschwindet - auch das Loch im C. Der
    # Schlagschatten der Kachel ist halbdurchsichtig und fliegt damit
    # ebenfalls raus; nur was ganz deckend war, bleibt.
    fest = np.clip((alpha - 0.85) / 0.1, 0.0, 1.0)
    durch = fest * marke
    bild = np.concatenate([np.clip(neu, 0, 255), durch * 255.0], axis=-1)
    Image.fromarray(bild.astype(np.uint8)).save(
        ZIEL / f"chattiflexii-{name}.png")
    print(f"  {name}")


def main() -> int:
    ZIEL.mkdir(parents=True, exist_ok=True)
    w, alpha = gewichte(vorlage())
    print("Varianten:")

    # Schlicht: Körper gedeckt, Bogen in der Farbe
    variante("violett", KOERPER, BOGEN, w, alpha)
    variante("blau", (126, 142, 160), (91, 200, 255), w, alpha)
    variante("gruen", (124, 150, 132), (98, 224, 138), w, alpha)
    variante("orange", (162, 142, 124), (255, 162, 75), w, alpha)
    variante("rosa", (168, 138, 152), (255, 119, 180), w, alpha)

    # Ganz durchgemustert
    variante("camouflage",
             flecken((78, 92, 56), [(52, 64, 38), (104, 118, 68), (78, 68, 46),
                                    (130, 140, 88)], zahl=70, streuung=7),
             flecken((138, 156, 88), [(96, 114, 60), (178, 186, 120),
                                      (110, 96, 62), (206, 208, 158)],
                     zahl=60, streuung=3),
             w, alpha)
    variante("mitternacht",
             sterne((46, 54, 104), zahl=70, streuung=9),
             sterne((84, 96, 178), zahl=150, streuung=5),
             w, alpha)
    variante("sonnenuntergang",
             verlauf((150, 76, 132), (84, 48, 104)),
             verlauf((255, 176, 84), (255, 92, 152)),
             w, alpha)
    variante("neon",
             (72, 76, 108),
             verlauf((80, 240, 255), (255, 80, 220)),
             w, alpha)
    # Über die ganze Marke, damit alle Farben darin liegen
    oben, unten = bereich(w, 2)[0], bereich(w, 1)[1]
    bogenfarben = [(232, 66, 66), (246, 156, 56), (248, 224, 76),
                   (88, 204, 100), (72, 144, 240), (154, 96, 224)]
    variante("regenbogen",
             verlauf_mehr(bogenfarben, schraeg=False, von=oben, bis=unten),
             verlauf_mehr(bogenfarben, schraeg=False, von=oben, bis=unten),
             w, alpha)

    # Was gerade überall zu sehen ist
    variante("chrom",
             verlauf_mehr([(186, 194, 206), (96, 108, 126), (212, 218, 228),
                           (80, 92, 112)]),
             verlauf_mehr([(246, 248, 252), (130, 146, 170), (252, 252, 255),
                           (104, 120, 146), (206, 216, 230)]),
             w, alpha)
    variante("holo",
             verlauf_mehr([(160, 226, 232), (206, 176, 236), (238, 186, 214)]),
             verlauf_mehr([(122, 238, 226), (150, 188, 255), (236, 160, 246),
                           (255, 214, 150), (150, 246, 220)]),
             w, alpha)
    variante("feuer",
             verlauf((188, 62, 30), (96, 28, 22)),
             verlauf_mehr([(255, 226, 92), (255, 150, 40), (226, 58, 40)]),
             w, alpha)
    variante("eis",
             verlauf((118, 156, 190), (72, 108, 146)),
             verlauf_mehr([(228, 248, 255), (140, 208, 246), (78, 160, 220)]),
             w, alpha)
    variante("aurora",
             verlauf((54, 82, 104), (40, 54, 86)),
             verlauf_mehr([(120, 248, 196), (86, 214, 232), (140, 150, 246),
                           (206, 132, 238)]),
             w, alpha)
    return 0


if __name__ == "__main__":
    sys.exit(main())
