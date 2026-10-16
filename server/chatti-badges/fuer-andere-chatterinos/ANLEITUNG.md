# ChattiFlexii-Badges in deinem Chatterino

Damit siehst du die Badges, die sich ChattiFlexii-Leute gemacht haben, und kannst
dir selbst eins hochladen, das sie bei dir sehen. Beim Drüberfahren steht bei
deinem „Sonkerino-Badge“.

Dauert etwa 5 Minuten: 2 Dateien kopieren, 4 Zeilen einfügen.

## 1. Dateien kopieren

`ChattiBadgesClient.hpp` und `ChattiBadgesClient.cpp` nach
`src/providers/chattibadges/` in deinem Chatterino.

Heißt dein Programm anders als „Sonkerino“, ändere oben in
`ChattiBadgesClient.cpp` die Zeile mit `CLIENT_NAME`.

## 2. Vier Zeilen einfügen

**`src/CMakeLists.txt`**: neben `providers/chatterino/ChatterinoBadges.cpp`

```cmake
        providers/chattibadges/ChattiBadgesClient.cpp
        providers/chattibadges/ChattiBadgesClient.hpp
```

**`src/messages/MessageBuilder.cpp`**: oben bei den anderen `#include`s

```cpp
#include "providers/chattibadges/ChattiBadgesClient.hpp"
```

und unter `builder.appendChatterinoBadges(userID);` (Suche nach dieser Zeile)

```cpp
    chattibadgesclient::appendTo(builder, userID);
```

**`src/controllers/commands/CommandController.cpp`**: oben bei den
`#include`s

```cpp
#include "providers/chattibadges/ChattiBadgesClient.hpp"
```

und unter `this->registerCommand("/clip", &commands::clip);`

```cpp
    this->registerCommand("/chattibadge", &chattibadgesclient::uploadCommand);
```

## 3. Bauen und ausprobieren

Neu bauen und starten. Dann in irgendeinem Chat:

```
/chattibadge ~/Pictures/mein-badge.gif
```

- Ein normales Bild wird automatisch 72 × 72 groß.
- Ein **animiertes GIF** geht so hoch, wie es ist: höchstens **240 × 240**
  und **1 MB**.

Im Chat steht danach „wartet auf Freigabe“. Sobald Flexii es freigibt,
sehen es alle mit ChattiFlexii, und du siehst es bei dir. Die Badges
erscheinen und verschwinden zusammen mit den Chatterino-Badges
(Einstellungen → Visible badges → Chatterino).
