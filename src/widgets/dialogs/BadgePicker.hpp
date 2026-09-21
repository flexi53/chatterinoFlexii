// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/twitch/TwitchWebBadges.hpp"

#include <QPointer>
#include <QWidget>

#include <functional>
#include <optional>

class QLabel;
class QVBoxLayout;

namespace chatterino {

/// Buttons -> Input: the badges to choose from, laid out like the chat
/// identity on twitch.tv - how your name looks with them, then a tile for
/// each badge: those of this channel, worn only here, apart from those worn
/// in every channel. A click wears it at once.
class BadgePicker : public QWidget
{
public:
    /// Opens next to @a anchor, on its screen, and says it is loading until
    /// setChoices fills it
    BadgePicker(const QString &channelName, const QString &channelId,
                QWidget *anchor);

    void setChoices(const webbadges::Choices &choices);

    /// Told what is now seen next to the name, whenever that changes
    std::function<void(const std::optional<webbadges::Badge> &)> onWorn;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Colors {
        QColor background;
        QColor border;
        QColor text;
        QColor dim;
        QColor hover;
        QColor accent;
        QColor error;
    };
    /// Twitch's own, light or dark as the theme is
    static Colors colors();

    void rebuild();
    void wear(const webbadges::Badge &badge, bool global);
    /// Sizes it to what it holds, then follow()
    void place();
    /// Next to the button, on its screen, by the size it has now
    void follow();

    QString channelName_;
    QString channelId_;
    QPointer<QWidget> anchor_;
    std::optional<webbadges::Choices> choices_;
    QVBoxLayout *content_{};
    QLabel *status_{};
};

}  // namespace chatterino
