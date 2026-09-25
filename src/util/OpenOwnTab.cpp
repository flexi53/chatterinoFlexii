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

#include <vector>

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

/// Takes away every tab that only opened because the browser went there -
/// all but @a keep, which is the one being shown right now. Tabs the user
/// opened are never touched, and neither is one they added a second chat to.
void closeTemporaryTabs(Notebook &notebook, SplitContainer *keep)
{
    std::vector<SplitContainer *> going;
    for (int i = 0; i < notebook.getPageCount(); i++)
    {
        auto *page = dynamic_cast<SplitContainer *>(notebook.getPageAt(i));
        if (page == nullptr || page == keep || !page->isTemporary())
        {
            continue;
        }
        if (page->getSplits().size() != 1)
        {
            // Someone made it their own in the meantime
            page->setTemporary(false);
            continue;
        }
        going.push_back(page);
    }

    for (auto *page : going)
    {
        notebook.removePage(page);
    }
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
                closeTemporaryTabs(notebook, page);
            }
            return true;
        }
    }

    if (!openWhenMissing)
    {
        if (closeOpened)
        {
            closeTemporaryTabs(notebook, nullptr);
        }
        return false;
    }

    auto channel = getApp()->getTwitch()->getOrAddChannel(name);
    auto *page = notebook.addPage(true);
    page->appendNewSplit(false)->setChannel(channel);
    if (closeOpened)
    {
        // Only there while the browser is on this channel, and never saved
        page->setTemporary(true);
        closeTemporaryTabs(notebook, page);
    }
    return true;
}

}  // namespace chatterino
