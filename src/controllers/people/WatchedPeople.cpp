// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/people/WatchedPeople.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/filters/FilterRecord.hpp"
#include "controllers/filters/lang/Filter.hpp"
#include "controllers/highlights/HighlightPhrase.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QRegularExpression>
#include <QUuid>

#include <algorithm>

namespace chatterino {

namespace {

/// What the filter the tab uses is called on the filters page
const QString FILTER_NAME = QStringLiteral("Leute im Blick");

/// What the tab itself is called - a mentions split would otherwise read
/// "/mentions"
const QString TAB_NAME = QStringLiteral("User");

/// Clear, so a watched message looks in the chat as it always did - the
/// highlight is only there to get it into the mentions channel
const QColor QUIET_COLOR{0, 0, 0, 0};

}  // namespace

QStringList WatchedPeople::read(const QString &written)
{
    static const QRegularExpression SEPARATORS{QStringLiteral(R"([,\n\r;\s]+)")};

    QStringList people;
    for (const auto &part : written.split(SEPARATORS, Qt::SkipEmptyParts))
    {
        auto name = part.trimmed().toLower();
        while (name.startsWith(u'@') || name.startsWith(u'#'))
        {
            name.remove(0, 1);
        }
        if (!name.isEmpty() && !people.contains(name))
        {
            people.append(name);
        }
    }
    return people;
}

QString WatchedPeople::write(const QStringList &people)
{
    return people.join(u'\n');
}

QStringList WatchedPeople::people()
{
    return read(getSettings()->watchedPeople.getValue());
}

bool WatchedPeople::watches(const QString &login, const QStringList &people)
{
    if (login.isEmpty())
    {
        return false;
    }
    const auto name = login.toLower();
    return std::any_of(people.begin(), people.end(),
                       [&name](const auto &watched) {
                           return watched == name;
                       });
}

bool WatchedPeople::watches(const QString &login)
{
    return watches(login, people());
}

bool WatchedPeople::toggle(const QString &login)
{
    auto name = login.trimmed().toLower();
    while (name.startsWith(u'@'))
    {
        name.remove(0, 1);
    }
    if (name.isEmpty())
    {
        return false;
    }

    auto chosen = people();
    const bool watched = chosen.contains(name);
    if (watched)
    {
        chosen.removeAll(name);
    }
    else
    {
        chosen.append(name);
    }
    getSettings()->watchedPeople.setValue(write(chosen));
    return !watched;
}

QString WatchedPeople::expressionFor(const QStringList &people)
{
    QStringList parts;
    for (const auto &name : people)
    {
        parts.append(QStringLiteral(R"((author.name == "%1"))").arg(name));
    }
    return parts.join(QStringLiteral(" || "));
}

QString WatchedPeople::problemWith(const QString &expression)
{
    const auto written = expression.trimmed();
    if (written.isEmpty())
    {
        return {};
    }

    auto result = filters::Filter::fromString(written);
    if (std::holds_alternative<filters::FilterError>(result))
    {
        return std::get<filters::FilterError>(result).message;
    }
    if (std::get<filters::Filter>(result).returnType() != filters::Type::Bool)
    {
        return QStringLiteral("Der Filter muss ja oder nein ergeben, etwa "
                              R"(author.name == "name".)");
    }
    return {};
}

namespace {

/// The expression the tab filters by: one written by hand where there is
/// one, the names otherwise
QString wantedExpression()
{
    const auto own = getSettings()->watchedPeopleFilter.getValue().trimmed();
    if (!own.isEmpty() && WatchedPeople::problemWith(own).isEmpty())
    {
        return own;
    }
    return WatchedPeople::expressionFor(WatchedPeople::people());
}

/// Puts @a expression on the filters page under its name, and gives the id
/// it is kept under - the same one from then on, so a tab keeps working
QUuid keepFilter(const QString &expression)
{
    auto &records = getSettings()->filterRecords;
    const auto kept = QUuid::fromString(
        getSettings()->watchedPeopleFilterId.getValue());

    for (int i = 0; i < records.raw().size(); i++)
    {
        const auto &record = records.raw()[i];
        if (record->getId() != kept)
        {
            continue;
        }
        if (record->getFilter() != expression ||
            record->getName() != FILTER_NAME)
        {
            // A record holds its filter for good, so it is replaced
            records.removeAt(i);
            records.insert(std::make_shared<FilterRecord>(
                               FILTER_NAME, expression, kept),
                           i);
        }
        return kept;
    }

    auto record = std::make_shared<FilterRecord>(FILTER_NAME, expression);
    records.append(record);
    getSettings()->watchedPeopleFilterId.setValue(
        record->getId().toString(QUuid::WithoutBraces));
    return record->getId();
}

/// A highlight that does nothing but let the message through to the
/// mentions channel
HighlightPhrase quietHighlight(const QString &name)
{
    return HighlightPhrase{
        name,          true,  false, false, false,
        false,         "",    QUIET_COLOR,
    };
}

/// Makes sure everyone in @a wanted has such a highlight, and takes away
/// those this page added earlier and nobody asks for any more
void keepHighlights(const QStringList &wanted)
{
    auto &highlights = getSettings()->highlightedUsers;
    const auto ours =
        WatchedPeople::read(getSettings()->watchedPeopleHighlights.getValue());

    // Away with ours that are no longer wanted - never with one the user
    // wrote themselves
    for (const auto &name : ours)
    {
        if (wanted.contains(name))
        {
            continue;
        }
        for (int i = highlights.raw().size() - 1; i >= 0; i--)
        {
            if (highlights.raw()[i].getPattern().compare(
                    name, Qt::CaseInsensitive) == 0)
            {
                highlights.removeAt(i);
            }
        }
    }

    QStringList kept;
    for (const auto &name : wanted)
    {
        const auto &raw = highlights.raw();
        const bool there =
            std::any_of(raw.begin(), raw.end(), [&name](const auto &phrase) {
                return phrase.getPattern().compare(name, Qt::CaseInsensitive) ==
                       0;
            });
        if (!there)
        {
            highlights.append(quietHighlight(name));
        }
        kept.append(name);
    }
    getSettings()->watchedPeopleHighlights.setValue(
        WatchedPeople::write(kept));
}

}  // namespace

void WatchedPeople::sync()
{
    const bool on = getSettings()->watchedPeopleEnabled;
    // Switched off, nothing of ours stays in the way: no highlight, no
    // message reaching the mentions channel because of us
    keepHighlights(on ? people() : QStringList{});

    const auto expression = wantedExpression();
    if (!expression.isEmpty())
    {
        keepFilter(expression);
    }
}

void WatchedPeople::openTab()
{
    sync();

    const auto expression = wantedExpression();
    if (expression.isEmpty())
    {
        return;
    }
    const auto id = keepFilter(expression);
    auto mentions = getApp()->getTwitch()->getMentionsChannel();

    auto &notebook = getApp()->getWindows()->getMainWindow().getNotebook();
    for (int i = 0; i < notebook.getPageCount(); i++)
    {
        auto *page = dynamic_cast<SplitContainer *>(notebook.getPageAt(i));
        if (page == nullptr)
        {
            continue;
        }
        for (auto *split : page->getSplits())
        {
            if (split->getChannel() == mentions &&
                split->getFilters().contains(id))
            {
                // Named once; a name of your own stays
                if (page->getTab() != nullptr &&
                    !page->getTab()->hasCustomTitle())
                {
                    page->getTab()->setCustomTitle(TAB_NAME);
                }
                notebook.select(page);
                return;
            }
        }
    }

    auto *page = notebook.addPage(true);
    auto *split = page->appendNewSplit(false);
    split->setChannel(mentions);
    split->setFilters({id});
    if (page->getTab() != nullptr)
    {
        page->getTab()->setCustomTitle(TAB_NAME);
    }
}

void WatchedPeople::start()
{
    auto &settings = *getSettings();
    // Kept in step from here on; the settings page changes these
    settings.watchedPeopleEnabled.connect([](auto, auto) {
        sync();
    });
    settings.watchedPeople.connect([](const auto &, auto) {
        sync();
    });
    settings.watchedPeopleFilter.connect([](const auto &, auto) {
        sync();
    });
}

}  // namespace chatterino
