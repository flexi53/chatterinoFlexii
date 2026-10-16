// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "singletons/Paths.hpp"
#include "widgets/BaseWindow.hpp"
#include "widgets/DraggablePopup.hpp"

#include <pajlada/signals/scoped-connection.hpp>
#include <pajlada/signals/signal.hpp>
#include <QPixmap>
#include <QPointer>

#include <chrono>
#include <vector>

class QCheckBox;
class QLineEdit;
class QPushButton;
class QMovie;

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;
struct Message;
using MessagePtr = std::shared_ptr<const Message>;
class Label;
class MarkdownLabel;
class EditUserNotesDialog;
class ChannelView;
class Split;
struct HelixUser;
class ModChannelsRow;
class LabelButton;
class PixmapButton;
class LiveIndicator;

class UserInfoPopup final : public DraggablePopup
{
    Q_OBJECT

public:
    /**
     * @param closeAutomatically Decides whether the popup should close when it loses focus
     * @param split Will be used as the popup's parent. Must not be null
     */
    UserInfoPopup(bool closeAutomatically, Split *split);

    void setData(const QString &name, const ChannelPtr &channel);
    void setData(const QString &name, const ChannelPtr &contextChannel,
                 const ChannelPtr &openingChannel);

protected:
    void themeChangedEvent() override;
    void scaleChangedEvent(float scale) override;
    void windowDeactivationEvent() override;

private:
    void installEvents();
    void updateUserData();
    void updateLatestMessages();
    void updateNotes();
    /// ChattiFlexii, Twitch-Extras: what the list at the bottom shows - what
    /// was seen here, what Twitch keeps for the moderators, or the notes
    /// they left
    enum class HistoryMode { Seen, Twitch, Comments };
    void showHistory(HistoryMode mode);
    /// Puts @a messages in the list instead of what was seen here
    void showOwnMessages(const std::vector<MessagePtr> &messages,
                         const QString &none);

    void loadAvatar(const QString &userID, const QString &pictureURL,
                    bool isKick);

    void loadSevenTVAvatar(const QString &userID, bool isKick);
    void setSevenTVAvatar(const QString &filename, const QByteArray &format);

    void saveCacheAvatar(const QByteArray &avatar,
                         const QString &filename) const;

    void updateAvatarUrl();

    void updateKickUserData();
    void onKickProfilePictureClick(Qt::MouseButton button);

    QStringView platformName() const;

    void appendCommonProfileActions(QMenu *menu);

    bool isMod_{};
    HistoryMode historyMode_ = HistoryMode::Seen;
    bool isBroadcaster_{};

    Split *split_;

    QString userName_;
    QString userId_;
    QString avatarUrl_;
    QString helixAvatarUrl_;
    QString seventvAvatarUrl_;
    QString seventvUserID_;

    QString kickUserSlug_;

    // The channel the popup was opened from (e.g. /mentions or #forsen). Can be a special channel.
    ChannelPtr channel_;

    // The channel the messages are rendered from (e.g. #forsen). Can be a special channel, but will try to not be where possible.
    ChannelPtr underlyingChannel_;

    pajlada::Signals::NoArgSignal userStateChanged_;

    std::unique_ptr<pajlada::Signals::ScopedConnection> refreshConnection_;
    std::unique_ptr<pajlada::Signals::ScopedConnection>
        userDataUpdatedConnection_;

    // If we should close the dialog automatically if the user clicks out
    // Set based on the "Automatically close usercard when it loses focus" setting
    // Pinned status is tracked in DraggablePopup::isPinned_.
    const bool closeAutomatically_;

    class TimeoutWidget;
    struct {
        PixmapButton *avatarButton = nullptr;
        PixmapButton *localizedNameCopyButton = nullptr;

        Label *nameLabel = nullptr;
        Label *localizedNameLabel = nullptr;
        Label *pronounsLabel = nullptr;
        Label *followerCountLabel = nullptr;
        Label *createdDateLabel = nullptr;
        Label *userIDLabel = nullptr;
        Label *followageLabel = nullptr;
        Label *subageLabel = nullptr;
        ModChannelsRow *modChannels = nullptr;

        LiveIndicator *liveIndicator = nullptr;

        QCheckBox *block = nullptr;
        QCheckBox *ignoreHighlights = nullptr;
        MarkdownLabel *notesPreview = nullptr;
        LabelButton *notesAdd = nullptr;

        Label *noMessagesLabel = nullptr;
        ChannelView *latestMessages = nullptr;
        /// ChattiFlexii: the switch above the list, and the line to leave a
        /// note under it - only for moderators with the Twitch app login
        QWidget *historySwitch = nullptr;
        QPushButton *historySeen = nullptr;
        QPushButton *historyComments = nullptr;
        QWidget *commentRow = nullptr;
        QLineEdit *commentField = nullptr;

        LabelButton *usercardLabel = nullptr;
        /// ChattiFlexii: their own chat as a tab, and their 7TV page
        LabelButton *theirChat = nullptr;
        LabelButton *seventvPage = nullptr;
        /// ChattiFlexii: editor or not, for the broadcaster - Twitch-Extras
        LabelButton *editorRole = nullptr;
        LabelButton *switchAvatars = nullptr;

        TimeoutWidget *timeoutWidget = nullptr;
    } ui_;

    QMovie *seventvAvatar_ = nullptr;
    bool isTwitchAvatarShown_ = true;
    QPixmap avatarPixmap_;
    QPointer<EditUserNotesDialog> editUserNotesDialog_;

    bool isKick_ = false;
    uint64_t kickUserID_ = 0;

    class TimeoutWidget : public BaseWidget
    {
    public:
        enum Action { Ban, Unban, Timeout, Warn };

        TimeoutWidget();

        pajlada::Signals::Signal<std::pair<Action, int>> buttonClicked;

        void setMinTimeout(int minSecs);
        /// ChattiFlexii: "Verwarnen" is Twitch's own, so only there
        void setWarnVisible(bool visible);

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        std::vector<std::pair<QWidget *, int>> timeoutButtons;
        QWidget *warnBox_ = nullptr;
    };
};

}  // namespace chatterino
