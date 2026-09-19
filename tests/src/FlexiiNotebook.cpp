// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Tests for how ChattiFlexii's tab groups work with the tab visibility
// filters. Kept apart from Flexii.cpp as a notebook needs an application
// with a window manager.

#include "controllers/hotkeys/HotkeyController.hpp"
#include "mocks/BaseApplication.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"

#include <QWidget>

using namespace chatterino;

namespace {

class MockApplication : public mock::BaseApplication
{
public:
    MockApplication()
        : windowManager(this->args, this->paths_, this->settings, this->theme,
                        this->fonts)
    {
    }

    HotkeyController *getHotkeys() override
    {
        return &this->hotkeys;
    }

    WindowManager *getWindows() override
    {
        return &this->windowManager;
    }

    HotkeyController hotkeys;
    WindowManager windowManager;
};

class TestNotebook : public Notebook
{
public:
    TestNotebook()
        : Notebook(nullptr)
    {
    }

    using Notebook::setTabVisibilityFilter;
};

/// Only live tabs: a live one, one of the always shown group "big5" that is
/// offline, and one offline that belongs to no group
class FlexiiNotebookFixture : public ::testing::Test
{
protected:
    FlexiiNotebookFixture()
    {
        this->notebook.addPage(&this->live, "live");
        auto *big5 = this->notebook.addPage(&this->big5Offline, "big5");
        this->notebook.addPage(&this->offline, "offline");
        this->notebook.addTabToGroup(big5, "big5");
        this->notebook.setTabGroupAlwaysVisible("big5", true);
        this->notebook.setTabVisibilityFilter([](const NotebookTab *tab) {
            return tab->getTitle() == "live";
        });
        this->notebook.select(&this->live);
    }

    MockApplication mockApplication;
    TestNotebook notebook;
    QWidget live;
    QWidget big5Offline;
    QWidget offline;
};

}  // namespace

TEST_F(FlexiiNotebookFixture, KeyboardReachesAlwaysShownGroupsOffline)
{
    EXPECT_EQ(this->notebook.getVisibleTabCount(), 2);

    this->notebook.selectNextTab();
    EXPECT_EQ(this->notebook.getSelectedPage(), &this->big5Offline);

    // Past it, the offline tab outside the group is skipped
    this->notebook.selectNextTab();
    EXPECT_EQ(this->notebook.getSelectedPage(), &this->live);

    this->notebook.selectVisibleIndex(1);
    EXPECT_EQ(this->notebook.getSelectedPage(), &this->big5Offline);
}
