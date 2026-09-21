// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/twitch/TwitchWebBadges.hpp"
#include "widgets/buttons/Button.hpp"

#include <QPixmap>
#include <QString>

#include <optional>

class QMenu;

namespace chatterino {

/// Buttons -> Input: chooses the badge you wear, as the chat on twitch.tv
/// does - one of this channel's, or one worn everywhere. Shows the badge
/// worn here once it is known. Needs the browser login kept under Buttons;
/// without it, it says so and where to put it.
class BadgeButton : public Button
{
public:
    explicit BadgeButton(BaseWidget *parent = nullptr);

    /// The channel it chooses for - the one its split shows
    void setChannel(const QString &name, const QString &id);

    /// Where a menu of @a menu's size opens for a button at @a button, both
    /// on the whole desktop: above it, or below when there is no room
    /// above, and always within @a screen - the screen the button is on. A
    /// spot past the screen's edge would open it on the one next to it.
    static QPoint placeMenu(const QRect &button, const QSize &menu,
                            const QRect &screen);

protected:
    void paintContent(QPainter &painter) override;
    void showEvent(QShowEvent *event) override;

private:
    /// Asks Twitch what can be chosen here and what is worn
    void refresh(bool thenOpen);
    void openMenu(const webbadges::Choices &choices);
    void wear(const webbadges::Badge &badge, bool global);
    void showWorn(const std::optional<webbadges::Badge> &badge);
    void refreshTooltip();
    /// Opens @a menu next to the button, on its screen
    void popup(QMenu *menu);

    QString name_;
    QString id_;
    /// The channel the badge worn is known for
    QString knownFor_;
    std::optional<webbadges::Badge> worn_;
    QPixmap picture_;
    bool asking_ = false;
};

}  // namespace chatterino
