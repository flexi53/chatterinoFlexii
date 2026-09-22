// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/OpenOwnTab.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
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

}  // namespace chatterino
