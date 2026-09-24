<!--
SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>

SPDX-License-Identifier: MIT
-->

# ChattiFlexii Tab-Folger

Eine winzige Browser-Erweiterung, die ChattiFlexii sagt, welchen Twitch-Kanal
du gerade anschaust. ChattiFlexii holt dann den Tab dieses Kanals nach vorne —
einzuschalten unter **Einstellungen → Aussehen → Tabs → Dem Browser folgen**.

Sie meldet sich, wenn du

- in einem Tab einen anderen Kanal aufrufst,
- **zwischen Browser-Tabs wechselst**,
- ein anderes Browserfenster nach vorne holst.

Das Letzte kann die offizielle Erweiterung („Chatterino Native Host“) nicht;
die ist außerdem auf Windows ausgerichtet, weil ihr Hauptzweck das Einbetten
des Chats ins Browserfenster ist. Diese hier baut nichts ein und liest nichts
aus der Seite — nur die Adresse des Tabs, der gerade vorne ist.

## Einbauen

1. In Brave (oder Chrome) `brave://extensions` bzw. `chrome://extensions`
   öffnen und rechts oben **Entwicklermodus** einschalten.
2. **Entpackte Erweiterung laden** und diesen Ordner auswählen.
3. ChattiFlexii einmal neu starten.

Mehr ist nicht nötig: Die Erweiterung hat eine feste Kennung
(`ienhobdhggbbfejemmienhdflcoleihp`), und ChattiFlexii lässt sie von sich aus
zu.

Die offizielle Erweiterung kannst du danach ausschalten — außer du nutzt unter
Windows das Einbetten des Chats.

## Warum eine feste Kennung

Entpackte Erweiterungen bekommen sonst je nach Ordner eine andere Kennung, und
ChattiFlexii nimmt nur Nachrichten von Erweiterungen an, die es kennt. Der
öffentliche Schlüssel in `manifest.json` legt die Kennung fest, egal wo der
Ordner liegt. Der private Schlüssel wird dafür nicht gebraucht und ist nirgends
abgelegt.
