// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWindow.hpp"

#include <boost/signals2.hpp>
#include <pajlada/settings/setting.hpp>
#include <pajlada/signals/signal.hpp>
#include <pajlada/signals/signalholder.hpp>

class QSplitter;

namespace chatterino {

class PixmapButton;
class LabelButton;
class Theme;
class UpdateDialog;
class SplitNotebook;
class Channel;

class SplitContainer;

enum class WindowType { Main, Popup, Attached };

class Window : public BaseWindow
{
    Q_OBJECT

public:
    explicit Window(WindowType type, QWidget *parent);

    WindowType getType();
    SplitNotebook &getNotebook();

    /// ChattiFlexii: the chats held at the bottom of the window. They stay
    /// where they are while the tabs above them change, which is what one
    /// otherwise keeps a second window open for.
    SplitContainer *getPinnedContainer();
    /// Whether anything is held down there
    bool hasPinnedSplits() const;
    /// ChattiFlexii: builds the lower part from the list that holds for
    /// every window - see controllers/splits/PinnedSplits.hpp
    void applyPinnedSplits();
    /// Shows or hides the lower part, depending on whether anything is in
    /// it, and gives it @a height pixels where one is asked for
    void refreshPinnedArea(int height = 0);
    /// How tall the lower part stands right now, 0 while it is empty
    int pinnedHeight() const;

    pajlada::Signals::NoArgSignal closed;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;
    /// ChattiFlexii: watches the part at the bottom, so it goes away once
    /// the last chat is taken out of it
    bool eventFilter(QObject *watched, QEvent *event) override;
    void themeChangedEvent() override;

private:
    void addCustomTitlebarButtons();
    void addDebugStuff(
        std::map<QString, std::function<QString(std::vector<QString>)>>
            &actions);
    void addShortcuts() override;
    void addLayout();
    void onAccountSelected();
    void addMenuBar();

    WindowType type_;

    SplitNotebook *notebook_;
    /// ChattiFlexii: the part that stays - one page, no tabs of its own
    SplitNotebook *pinnedNotebook_{};
    QSplitter *splitter_{};
    LabelButton *userLabel_ = nullptr;
    std::shared_ptr<UpdateDialog> updateDialogHandle_;

    pajlada::Signals::SignalHolder signalHolder_;
    std::vector<boost::signals2::scoped_connection> bSignals_;

    // this is only used on Windows and only on the main window, for the one used otherwise, see SplitNotebook in Notebook.hpp
    PixmapButton *streamerModeTitlebarIcon_ = nullptr;
    void updateStreamerModeIcon();

    friend class Notebook;
};

}  // namespace chatterino
