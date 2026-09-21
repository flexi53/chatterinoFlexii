// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/twitch/TwitchWebBadges.hpp"
#include "widgets/buttons/Button.hpp"

#include <pajlada/signals/scoped-connection.hpp>
#include <QPixmap>
#include <QPointer>
#include <QString>

#include <memory>
#include <optional>

namespace chatterino {

class Channel;
class BadgePicker;

/// Buttons -> Input: chooses the badge you wear, as the chat on twitch.tv
/// does - one of this channel's, or one worn everywhere. Shows the badge
/// worn here once it is known. Needs the browser login kept under Buttons;
/// without it, it says so and where to put it.
class BadgeButton : public Button
{
public:
    explicit BadgeButton(BaseWidget *parent = nullptr);

    /// The channel it chooses for - the one its split shows
    void setChannel(const std::shared_ptr<Channel> &channel);

    /// Where a window of @a menu's size opens for a button at @a button,
    /// both on the whole desktop: above it, or below when there is no room
    /// above, and always within @a screen - the screen the button is on. A
    /// spot past the screen's edge would open it on the one next to it.
    static QPoint placeMenu(const QRect &button, const QSize &menu,
                            const QRect &screen);

protected:
    void paintContent(QPainter &painter) override;
    void showEvent(QShowEvent *event) override;

private:
    /// Twitch's id of the channel - known a moment after it was joined
    QString channelId() const;
    /// Asks Twitch what can be chosen here and what is worn
    void refresh();
    void openPicker();
    void showWorn(const std::optional<webbadges::Badge> &badge);
    void refreshTooltip();

    std::weak_ptr<Channel> channel_;
    QString name_;
    /// The channel the badge worn is known for
    QString knownFor_;
    std::optional<webbadges::Badge> worn_;
    QPixmap picture_;
    bool asking_ = false;
    QPointer<BadgePicker> picker_;
    std::optional<pajlada::Signals::ScopedConnection> joined_;
};

}  // namespace chatterino
