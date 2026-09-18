// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/SpellingVariants.hpp"

#include <QStringList>

#include <algorithm>
#include <array>

namespace chatterino::spelling {

namespace {

struct Letter {
    /// Digits and signs written for it
    QStringView leet;
    /// Letters from other alphabets, or with accents, that look like it
    QStringView lookalikes;
};

// clang-format off
const std::array<Letter, 26> LETTERS{{
    /* a */ {u"4@", u"аàáâäãåąα"},
    /* b */ {u"8", u"ь"},
    /* c */ {u"", u"сçćčϲ"},
    /* d */ {u"", u"ԁď"},
    /* e */ {u"3€", u"еёèéêëęε"},
    /* f */ {u"", u"ƒ"},
    /* g */ {u"96", u"ɡğ"},
    /* h */ {u"", u"һн"},
    /* i */ {u"1!|", u"іїìíîïıɪl"},
    /* j */ {u"", u"ј"},
    /* k */ {u"", u"кκ"},
    /* l */ {u"1|", u"ӏłιi"},
    /* m */ {u"", u"м"},
    /* n */ {u"", u"пñńη"},
    /* o */ {u"0", u"оοòóôöõøőσ"},
    /* p */ {u"", u"рρ"},
    /* q */ {u"", u"ԛ"},
    /* r */ {u"", u"гř"},
    /* s */ {u"5$", u"ѕšśş"},
    /* t */ {u"7+", u"тτť"},
    /* u */ {u"", u"υùúûüűų"},
    /* v */ {u"", u"νѵ"},
    /* w */ {u"", u"ѡω"},
    /* x */ {u"", u"хχ×"},
    /* y */ {u"", u"уýÿγ"},
    /* z */ {u"2", u"žźż"},
}};
// clang-format on

/// What may stand between the letters. None of it is a sign that stands for
/// a letter, so the two never compete for the same character.
const QString GAP = QStringLiteral(R"([\s.,_\-*~'"^:;/\\#=·•]{0,3})");
/// What may stand between the words of a phrase, including nothing
const QString WORD_GAP = QStringLiteral(R"([^\p{L}\p{N}]*)");

bool isAsciiLetter(QChar c)
{
    return c >= u'a' && c <= u'z';
}

const Letter &letter(QChar c)
{
    return LETTERS.at(c.unicode() - u'a');
}

/// @a c as it is written inside a character class
QString inClass(QChar c)
{
    static const QString special = QStringLiteral("\\]^-[");
    return special.contains(c) ? QStringLiteral("\\") + c : QString(c);
}

/// Everything that may stand for the Latin letter @a c, and @a extra
QString letterClass(QChar c, const Options &options, QChar extra = {})
{
    QString chars(c);
    if (options.leet)
    {
        chars += letter(c).leet;
    }
    if (options.lookalikes)
    {
        chars += letter(c).lookalikes;
    }
    if (!extra.isNull() && !chars.contains(extra))
    {
        chars += extra;
    }
    if (chars.size() == 1)
    {
        return QRegularExpression::escape(chars);
    }

    QString result = QStringLiteral("[");
    for (const auto ch : chars)
    {
        result += inClass(ch);
    }
    return result + u']';
}

/// The Latin letter @a c is written as, without its accent - e for é
QChar baseLetter(QChar c)
{
    const auto decomposed = QString(c).normalized(QString::NormalizationForm_D);
    const auto base = decomposed.isEmpty() ? c : decomposed.front();
    return isAsciiLetter(base) ? base : QChar();
}

/// One letter of the word: the key it is collapsed by, and what finds it
struct Unit {
    QString key;
    QString regex;
};

Unit unitFor(QChar c, const Options &options)
{
    const auto gap = options.separated ? GAP : QString();
    // ä is also written ae - or a.e, when there may be something between
    // the letters - or a
    const auto umlaut = [&options, &gap, c](QChar base) {
        return QStringLiteral("(?:%1%3%2|%1)")
            .arg(letterClass(base, options, c), letterClass(u'e', options),
                 gap);
    };
    switch (c.unicode())
    {
        case u'ä':
            return {c, umlaut(u'a')};
        case u'ö':
            return {c, umlaut(u'o')};
        case u'ü':
            return {c, umlaut(u'u')};
        case u'ß':
            return {c, QStringLiteral("(?:ß|%1(?:%2%1)?)")
                           .arg(letterClass(u's', options), gap)};
        default:
            break;
    }

    if (isAsciiLetter(c))
    {
        return {c, letterClass(c, options)};
    }
    if (const auto base = baseLetter(c); !base.isNull())
    {
        return {base, letterClass(base, options, c)};
    }
    return {c, QRegularExpression::escape(QString(c))};
}

QString wordPattern(const QString &word, const Options &options)
{
    std::vector<Unit> units;
    for (const auto c : word)
    {
        auto unit = unitFor(c, options);
        // A double letter is found as one letter stretched, so written
        // single or three times over it is still found
        if (options.stretched && !units.empty() && units.back().key == unit.key)
        {
            continue;
        }
        units.push_back(std::move(unit));
    }

    QStringList parts;
    for (const auto &unit : units)
    {
        if (!options.stretched)
        {
            parts.append(unit.regex);
        }
        else if (options.separated)
        {
            // l, ll or l.l - stretched with something in between, too
            parts.append(QStringLiteral("%1(?:%2%1)*").arg(unit.regex, GAP));
        }
        else
        {
            parts.append(unit.regex.size() == 1 ||
                                 unit.regex.startsWith(u'[') ||
                                 unit.regex.startsWith(u'\\')
                             ? unit.regex + u'+'
                             : QStringLiteral("(?:%1)+").arg(unit.regex));
        }
    }
    return parts.join(options.separated ? GAP : QString());
}

}  // namespace

QString pattern(const QString &text, const Options &options)
{
    const auto words = text.toLower().split(
        QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
    if (words.isEmpty())
    {
        return {};
    }

    QStringList parts;
    for (const auto &word : words)
    {
        parts.append(wordPattern(word, options));
    }

    auto name = words.join(u' ');
    name.remove(u')');
    auto result = QStringLiteral("(?#%1)").arg(name);
    if (options.wholeWord)
    {
        result += QStringLiteral(R"((?<![\p{L}\p{N}]))");
    }
    result += parts.join(WORD_GAP);
    if (options.wholeWord)
    {
        result += QStringLiteral(R"((?![\p{L}\p{N}]))");
    }
    return result;
}

QRegularExpression compile(const QString &pattern)
{
    return QRegularExpression(pattern,
                              QRegularExpression::UseUnicodePropertiesOption |
                                  QRegularExpression::CaseInsensitiveOption);
}

std::vector<Example> examples(const QString &text, const Options &options)
{
    const auto word = text.trimmed().toLower();
    std::vector<Example> result;
    if (word.isEmpty())
    {
        return result;
    }
    const auto add = [&result, &word](const QString &example,
                                      const QString &note = {}) {
        if (example != word &&
            std::none_of(result.begin(), result.end(), [&](const auto &e) {
                return e.text == example;
            }))
        {
            result.push_back({example, note});
        }
    };

    if (options.leet)
    {
        QString leet;
        for (const auto c : word)
        {
            leet += isAsciiLetter(c) && !letter(c).leet.isEmpty()
                        ? letter(c).leet.front()
                        : c;
        }
        add(leet);
    }

    static const QString vowels = QStringLiteral("aeiouäöü");
    if (options.stretched)
    {
        // The first vowel three times over
        for (qsizetype i = 0; i < word.size(); i++)
        {
            if (vowels.contains(word[i]))
            {
                add(QString(word).insert(i, QString(2, word[i])));
                break;
            }
        }
        // A double letter left single
        for (qsizetype i = 1; i < word.size(); i++)
        {
            if (word[i] == word[i - 1] && word[i].isLetter())
            {
                add(QString(word).remove(i, 1));
                break;
            }
        }
    }

    if (options.separated && !word.contains(u' '))
    {
        QStringList letters;
        for (const auto c : word)
        {
            letters.append(QString(c));
        }
        add(letters.join(u'.'));
    }

    // German umlauts written out
    QString writtenOut = word;
    writtenOut.replace(u'ä', QStringLiteral("ae"))
        .replace(u'ö', QStringLiteral("oe"))
        .replace(u'ü', QStringLiteral("ue"))
        .replace(u'ß', QStringLiteral("ss"));
    add(writtenOut);

    if (options.lookalikes)
    {
        // A Latin letter swapped for a Cyrillic one that looks the same
        for (qsizetype i = 0; i < word.size(); i++)
        {
            const auto c = word[i];
            if (isAsciiLetter(c) && !letter(c).lookalikes.isEmpty() &&
                letter(c).lookalikes.front().script() == QChar::Script_Cyrillic)
            {
                add(QString(word).replace(i, 1, letter(c).lookalikes.front()),
                    QStringLiteral("mit kyrillischem %1")
                        .arg(letter(c).lookalikes.front()));
                break;
            }
        }
    }

    return result;
}

}  // namespace chatterino::spelling
