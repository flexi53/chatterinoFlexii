// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWidget.hpp"
#include "widgets/splits/HeaderParts.hpp"
#include "widgets/TooltipWidget.hpp"

#include <boost/signals2.hpp>
#include <pajlada/settings/setting.hpp>
#include <pajlada/signals/connection.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QMenu>
#include <QPoint>

#include <memory>
#include <vector>

namespace chatterino {

class SvgButton;
class DrawnButton;
class HeaderPicture;
class ActivityGraph;
class HeaderTitle;
class LabelButton;
class Label;
class Split;

class SplitHeader final : public BaseWidget
{
    Q_OBJECT

public:
    explicit SplitHeader(Split *split);

    void setAddButtonVisible(bool value);

    void updateChannelText();
    /// ChattiFlexii: the numbers the title bar shows beside the stream's
    /// own - only those switched on, and only where they can be had
    headerparts::Extras channelNumbers(TwitchChannel *channel) const;
    /// Buttons -> Titelleiste: how wide each part is drawn and how much
    /// room stands between them
    void applyPartWidths(int button, int addButton, float scale);
    void updateIcons();
    // Invoked when SplitHeader should update anything refering to a TwitchChannel's mode
    // has changed (e.g. sub mode toggled)
    void updateRoomModes();

protected:
    void scaleChangedEvent(float scale) override;
    void themeChangedEvent() override;

    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void initializeLayout();
    /// Hands the curve its share of the room beside the title - by default
    /// half of what the title does not need, so a short title does not
    /// float in a wide gap
    void fitActivity();
    /// Buttons -> Title bar: puts the parts in the order asked for
    void arrangeParts();
    std::unique_ptr<QMenu> createMainMenu();
    std::unique_ptr<QMenu> createChatModeMenu();

    /**
     * @brief   Reset the thumbnail data and timer so a new
     *          thumbnail can be fetched
     **/
    void resetThumbnail();

    void handleChannelChanged();
    /// Look -> Tabs: the channel's picture, the cover of what it streams and
    /// the activity curve, as far as they are switched on
    void updatePictures();

    Split *const split_{};
    QString tooltipText_{};
    TooltipWidget *const tooltipWidget_{};
    bool isLive_{false};
    QString thumbnail_;
    QElapsedTimer lastThumbnail_;
    std::chrono::steady_clock::time_point lastReloadedChannelEmotes_;
    std::chrono::steady_clock::time_point lastReloadedSubEmotes_;

    // ui
    DrawnButton *dropdownButton_{};
    HeaderTitle *titleLabel_{};

    LabelButton *modeButton_{};
    QAction *modeActionSetEmote{};
    QAction *modeActionSetSub{};
    QAction *modeActionSetSlow{};
    QAction *modeActionSetR9k{};
    QAction *modeActionSetFollowers{};

    SvgButton *moderationButton_{};
    SvgButton *chattersButton_{};
    SvgButton *trackerButton_{};
    DrawnButton *addButton_{};
    /// Whether this split is the one the plus belongs to
    bool addButtonWanted_{false};
    /// The bit of room after the title, which goes wherever it goes
    BaseWidget *titleSpace_{};
    QHBoxLayout *partsLayout_{};

    HeaderPicture *channelPicture_{};
    HeaderPicture *coverPicture_{};
    ActivityGraph *activity_{};
    /// Whose picture and which cover are shown, or being loaded
    QString pictureLogin_;
    QString coverGameId_;

    // states
    QPoint dragStart_{};
    bool dragging_{false};
    bool doubleClicked_{false};
    bool menuVisible_{false};

    // managedConnections_ contains connections for signals that are not managed by us
    // and don't change when the parent Split changes its underlying channel
    pajlada::Signals::SignalHolder managedConnections_;
    pajlada::Signals::SignalHolder channelConnections_;
    std::vector<boost::signals2::scoped_connection> bSignals_;

public Q_SLOTS:
    void reloadChannelEmotes();
    void reloadSubscriberEmotes();
    void reconnect();
};

}  // namespace chatterino
