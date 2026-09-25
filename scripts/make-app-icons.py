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


def helligkeit(flaeche_: np.ndarray) -> float:
    """Wie hell eine Fläche im Mittel ist, von 0 bis 1."""
    return float((flaeche_ @ np.array([0.299, 0.587, 0.114])).mean()) / 255.0


def variante(name: str, kachel, w, alpha, marke=None) -> None:
    """Schreibt eine Variante.

    Die Kachel trägt die Farbe oder das Muster und füllt das ganze Symbol -
    macOS legt ein durchsichtiges Programmsymbol sonst auf eine eigene
    helle Platte, und dann sähe die Datei anders aus als das laufende
    Programm. Die Marke darauf wird von selbst hell oder dunkel, je
    nachdem, worauf sie liegt.
    """
    grund = flaeche(kachel) if isinstance(kachel, tuple) else kachel
    if marke is None:
        marke = (255, 255, 255) if helligkeit(grund) < 0.55 else (28, 28, 34)
    vorn = flaeche(marke) if isinstance(marke, tuple) else marke
    # Der Körper etwas zurückgenommen, wie das Grau im Original
    hinten = 0.62 * vorn + 0.38 * grund

    neu = (w[..., 0:1] * grund + w[..., 1:2] * hinten + w[..., 2:3] * vorn)
    bild = np.concatenate([np.clip(neu, 0, 255), alpha * 255.0], axis=-1)
    Image.fromarray(bild.astype(np.uint8)).save(
        ZIEL / f"chattiflexii-{name}.png")
    print(f"  {name}")


def main() -> int:
    ZIEL.mkdir(parents=True, exist_ok=True)
    w, alpha = gewichte(vorlage())
    print("Varianten:")

    oben, unten = 0.08, 0.92

    # So, wie das Programm ausgeliefert wird
    variante("klassisch", KACHEL, w, alpha, marke=BOGEN)

    # Eine Farbe, wie sie ist
    variante("violett", verlauf((198, 104, 232), (126, 62, 178)), w, alpha)
    variante("blau", verlauf((86, 178, 255), (38, 104, 214)), w, alpha)
    variante("gruen", verlauf((86, 214, 130), (26, 148, 96)), w, alpha)
    variante("orange", verlauf((255, 170, 76), (232, 108, 38)), w, alpha)
    variante("rosa", verlauf((255, 132, 186), (226, 62, 134)), w, alpha)

    # Ganz durchgemustert
    variante("camouflage",
             flecken((86, 100, 60), [(56, 68, 40), (112, 126, 74),
                                     (78, 68, 46), (140, 150, 96)],
                     zahl=80, streuung=7),
             w, alpha)
    variante("mitternacht",
             sterne((22, 26, 58), zahl=200, streuung=5),
             w, alpha)
    variante("sonnenuntergang",
             verlauf_mehr([(255, 186, 96), (255, 106, 132), (138, 62, 156)]),
             w, alpha)
    variante("neon",
             verlauf_mehr([(24, 24, 40), (46, 210, 232), (226, 62, 200),
                           (24, 24, 40)]),
             w, alpha)
    variante("regenbogen",
             verlauf_mehr([(232, 66, 66), (246, 156, 56), (248, 224, 76),
                           (88, 204, 100), (72, 144, 240), (154, 96, 224)],
                          schraeg=False, von=oben, bis=unten),
             w, alpha)

    # Was gerade überall zu sehen ist
    variante("chrom",
             verlauf_mehr([(228, 232, 238), (128, 140, 158), (244, 246, 250),
                           (104, 116, 136), (206, 214, 226)]),
             w, alpha)
    variante("holo",
             verlauf_mehr([(140, 246, 232), (150, 188, 255), (236, 160, 246),
                           (255, 214, 150), (150, 246, 220)]),
             w, alpha)
    variante("feuer",
             verlauf_mehr([(255, 216, 88), (255, 138, 36), (206, 44, 36)]),
             w, alpha)
    variante("eis",
             verlauf_mehr([(232, 250, 255), (150, 210, 246), (74, 150, 214)]),
             w, alpha)
    variante("aurora",
             verlauf_mehr([(46, 60, 96), (86, 226, 182), (94, 156, 244),
                           (168, 108, 232)]),
             w, alpha)
    return 0


if __name__ == "__main__":
    sys.exit(main())
