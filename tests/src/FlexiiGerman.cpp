// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/German.hpp"

#include <gtest/gtest.h>
#include <QString>

using namespace chatterino;

namespace {

/// A text that is in the dictionary, with what it should say
constexpr auto KNOWN_ENGLISH = "General";
constexpr auto KNOWN_GERMAN = "Allgemein";

}  // namespace

TEST(FlexiiGerman, ATextInTheDictionaryIsSaidInGerman)
{
    EXPECT_EQ(german::say(KNOWN_ENGLISH), KNOWN_GERMAN);
}

TEST(FlexiiGerman, ATextWithNoEntryStaysAsItIs)
{
    // Nothing is lost when an entry is missing - that is what lets the
    // dictionary grow a piece at a time
    EXPECT_EQ(german::say("Some setting nobody has translated"),
              "Some setting nobody has translated");
    EXPECT_EQ(german::say(""), "");
}

TEST(FlexiiGerman, TheColonAndSpacesTheCallerAddedAreKept)
{
    EXPECT_EQ(german::say(QString(KNOWN_ENGLISH) + ":"),
              QString(KNOWN_GERMAN) + ":");
    EXPECT_EQ(german::say(QString(" ") + KNOWN_ENGLISH + " "),
              QString(" ") + KNOWN_GERMAN + " ");
    EXPECT_EQ(german::say(QString(KNOWN_ENGLISH) + " :"),
              QString(KNOWN_GERMAN) + " :");
}

TEST(FlexiiGerman, ATooltipBrokenIntoLinesIsStillFound)
{
    // Tooltips reach the dictionary already broken into lines, while it
    // holds them as one line
    const auto english =
        QStringLiteral("Show messages for timeouts, bans, and other\n"
                       "moderator actions.");
    const auto german = german::say(english);
    EXPECT_NE(german, english);
    EXPECT_FALSE(german.contains("moderator actions"));
}

TEST(FlexiiGerman, TheSearchKnowsBothTexts)
{
    const auto both = german::bothWords(KNOWN_ENGLISH);
    ASSERT_EQ(both.size(), 2);
    EXPECT_EQ(both[0], KNOWN_ENGLISH);
    EXPECT_EQ(both[1], KNOWN_GERMAN);

    // A text with no entry is only itself, not the same word twice
    const auto one = german::bothWords("Some setting nobody has translated");
    ASSERT_EQ(one.size(), 1);
}
