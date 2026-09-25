// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/OpenOwnTab.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QPointer>

namespace chatterino {

void openOwnTab(const std::shared_ptr<Channel> &channel)
{
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
            if (split->getChannel() == channel)
            {
                notebook.select(page);
                return;
            }
        }
    }
    // Its own channel - no dialog asking for a Twitch one
    notebook.addPage(true)->appendNewSplit(false)->setChannel(channel);
}

namespace {

/// The tab this opened by itself while following the browser, so it can be
/// taken away again once the browser is elsewhere. Nothing the user opened
/// is ever remembered here.
QPointer<SplitContainer> openedByFollowing;

/// Takes that tab away, as long as it is still the one and only thing that
/// was put there
void closeWhatWasOpened(Notebook &notebook, SplitContainer *keep)
{
    auto *page = openedByFollowing.data();
    openedByFollowing = nullptr;
    if (page == nullptr || page == keep)
    {
        return;
    }
    // Someone added a chat to it in the meantime - then it is theirs now
    if (page->getSplits().size() != 1)
    {
        return;
    }
    notebook.removePage(page);
}

}  // namespace

bool showChannelTab(const QString &name, const bool openWhenMissing,
                    const bool closeOpened)
{
    if (name.isEmpty())
    {
        return false;
    }

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
            const auto channel = split->getChannel();
            if (channel == nullptr ||
                channel->getType() != Channel::Type::Twitch)
            {
                continue;
            }
            if (channel->getName().compare(name, Qt::CaseInsensitive) != 0)
            {
                continue;
            }

            // Only the page is brought forward - the window stays where it
            // is, so the browser keeps the focus
            notebook.select(page, false);
            page->setSelected(split);
            if (closeOpened)
            {
                closeWhatWasOpened(notebook, page);
            }
            return true;
        }
    }

    if (!openWhenMissing)
    {
        if (closeOpened)
        {
            closeWhatWasOpened(notebook, nullptr);
        }
        return false;
    }

    auto channel = getApp()->getTwitch()->getOrAddChannel(name);
    auto *page = notebook.addPage(true);
    page->appendNewSplit(false)->setChannel(channel);
    if (closeOpened)
    {
        // The one before it goes, this one takes its place
        closeWhatWasOpened(notebook, page);
        openedByFollowing = page;
    }
    return true;
}

}  // namespace chatterino
