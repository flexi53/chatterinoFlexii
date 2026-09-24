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

bool showChannelTab(const QString &name, const bool openWhenMissing)
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
            return true;
        }
    }

    if (!openWhenMissing)
    {
        return false;
    }

    auto channel = getApp()->getTwitch()->getOrAddChannel(name);
    notebook.addPage(true)->appendNewSplit(false)->setChannel(channel);
    return true;
}

}  // namespace chatterino
