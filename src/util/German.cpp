// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/German.hpp"

#include <QAbstractButton>
#include <QFile>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QTabWidget>
#include <QTextStream>
#include <QWidget>

#include <algorithm>

namespace chatterino::german {

namespace {

/// Where a tooltip is broken into the next line, as Chatterino does it
constexpr int TOOLTIP_LINE = 50;
const QRegularExpression TOOLTIP_LINE_REGEX(
    QStringLiteral(R"(.{%1}\S*\K(\s+))").arg(TOOLTIP_LINE));

/// English -> German. Sorted by where it shows up.
const QHash<QString, QString> &words()
{
    static const QHash<QString, QString> words{
#include "util/GermanWords.inc"
    };
    return words;
}

/// Writes texts with no entry to the file in CHATTIFLEXII_GERMAN_MISSING,
/// each once
/// @a german broken into lines no longer than the longest line of
/// @a english, on spaces - so a text that was kept narrow stays narrow
QString brokenLike(const QString &german, const QString &english)
{
    qsizetype longest = 0;
    for (const auto &line : english.split('\n'))
    {
        longest = std::max(longest, line.length());
    }
    if (longest <= 0 || german.length() <= longest)
    {
        return german;
    }

    QString broken;
    qsizetype room = longest;
    for (const auto &word : german.split(' '))
    {
        if (broken.isEmpty())
        {
            broken = word;
            room = longest - word.length();
            continue;
        }
        if (word.length() + 1 > room)
        {
            broken += '\n' + word;
            room = longest - word.length();
            continue;
        }
        broken += ' ' + word;
        room -= word.length() + 1;
    }
    return broken;
}

void noteMissing(const QString &english)
{
    static const QString path =
        qEnvironmentVariable("CHATTIFLEXII_GERMAN_MISSING");
    if (path.isEmpty())
    {
        return;
    }

    static QMutex mutex;
    static QSet<QString> seen;
    const QMutexLocker locker(&mutex);
    if (seen.contains(english))
    {
        return;
    }
    seen.insert(english);

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream out(&file);
        // One line each, newlines kept readable
        out << QString(english).replace('\n', QStringLiteral("\\n")) << '\n';
    }
}

}  // namespace

QString say(const QString &english)
{
    if (english.isEmpty())
    {
        return english;
    }

    const auto found = words().find(english);
    if (found != words().end())
    {
        return found.value();
    }

    // The caller often adds a colon or spaces around the text, so the entry
    // is looked up without them and they are put back on
    qsizetype start = 0;
    qsizetype end = english.size();
    while (start < end && english[start].isSpace())
    {
        start++;
    }
    while (end > start &&
           (english[end - 1].isSpace() || english[end - 1] == ':'))
    {
        end--;
    }
    const auto core = english.mid(start, end - start);
    if (core != english)
    {
        const auto inner = words().find(core);
        if (inner != words().end())
        {
            return english.left(start) + inner.value() + english.mid(end);
        }
    }

    // Some texts come with their lines already broken; the dictionary holds
    // them as one line. The German text is broken again the same way - a
    // label or a column title that Chatterino kept narrow on purpose would
    // otherwise stretch the whole window.
    if (english.contains('\n'))
    {
        auto oneLine = core;
        oneLine.replace('\n', ' ');
        const auto flat = words().find(oneLine);
        if (flat != words().end())
        {
            return english.left(start) + brokenLike(flat.value(), core) +
                   english.mid(end);
        }
        noteMissing(oneLine);
        return english;
    }

    noteMissing(core.isEmpty() ? english : core);
    return english;
}

void translateWidgets(QWidget *root)
{
    if (root == nullptr)
    {
        return;
    }

    auto retell = [](const QString &text) {
        return text.isEmpty() ? text : say(text);
    };

    // Tooltips arrive already broken into lines, while the dictionary holds
    // them as one line - so it is asked with one line and broken again
    auto retellTooltip = [](const QString &text) {
        if (!text.contains('\n'))
        {
            return say(text);
        }
        auto oneLine = text;
        oneLine.replace('\n', ' ');
        auto german = say(oneLine);
        if (german == oneLine)
        {
            return text;  // no entry, keep the lines it came with
        }
        if (german.length() > TOOLTIP_LINE)
        {
            german.replace(TOOLTIP_LINE_REGEX, "\n");
        }
        return german;
    };

    for (auto *widget : root->findChildren<QWidget *>())
    {
        if (auto *label = qobject_cast<QLabel *>(widget))
        {
            label->setText(retell(label->text()));
        }
        else if (auto *button = qobject_cast<QAbstractButton *>(widget))
        {
            button->setText(retell(button->text()));
        }
        else if (auto *group = qobject_cast<QGroupBox *>(widget))
        {
            group->setTitle(retell(group->title()));
        }
        else if (auto *edit = qobject_cast<QLineEdit *>(widget))
        {
            edit->setPlaceholderText(retell(edit->placeholderText()));
        }
        else if (auto *tabs = qobject_cast<QTabWidget *>(widget))
        {
            for (int i = 0; i < tabs->count(); i++)
            {
                tabs->setTabText(i, retell(tabs->tabText(i)));
            }
        }

        widget->setToolTip(retellTooltip(widget->toolTip()));
    }
}

QStringList bothWords(const QString &english)
{
    auto german = say(english);
    if (german == english)
    {
        return {english};
    }
    return {english, german};
}

}  // namespace chatterino::german
