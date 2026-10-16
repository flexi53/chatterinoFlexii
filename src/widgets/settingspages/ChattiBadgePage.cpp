// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ChattiBadgePage.hpp"

#include "providers/chattibadges/ChattiBadges.hpp"
#include "providers/twitch/TwitchWebBadges.hpp"
#include "singletons/Settings.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>
#include <memory>
#include <vector>

namespace chatterino {

ChattiBadgePage::ChattiBadgePage()
    : view_(GeneralPageView::withoutNavigation(this))
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(this->view_);

    this->initOwnBadge(*this->view_);
    this->view_->addStretch();
}

void ChattiBadgePage::initOwnBadge(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Dein ChattiFlexii-Badge");
    layout.addDescription(
        "Ein Bild deiner Wahl neben deinem Namen - für alle, die ChattiFlexii "
        "benutzen. Es wird auf 72 × 72 gebracht und hochgeladen; sehen "
        "können es die anderen, sobald es freigegeben ist. Anzeigen lassen "
        "es sich unter General → Visible badges → ChattiFlexii.");
    layout.addDescription(
        "Damit niemand eins für einen fremden Namen hochlädt, fragt der "
        "Badge-Server einmal bei Twitch nach, wem dein Login gehört. "
        "Behalten wird der Login dort nicht.");

    SettingWidget::lineEdit("Badge-Server", s.chattiBadgesServer,
                            "https://chatti-badges.….workers.dev")
        ->setTooltip("Wo die Badges liegen. Steht schon richtig da, wenn "
                     "ChattiFlexii so ausgeliefert wurde.")
        ->addKeywords({"badge", "server", "chatti"})
        ->addTo(layout);

    // What is shown now, what was picked, and what can be done with it
    auto *preview = new QLabel;
    preview->setFixedSize(40, 40);
    preview->setAlignment(Qt::AlignCenter);
    preview->setStyleSheet(
        "border: 1px solid rgba(128, 128, 128, 90); border-radius: 6px;");
    auto *status = new QLabel;
    status->setWordWrap(true);
    auto *pick = new QPushButton("Bild wählen …");
    auto *send = new QPushButton("Hochladen");
    send->setEnabled(false);
    auto *remove = new QPushButton("Entfernen");

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(preview);
    row->addWidget(status, 1);
    row->addWidget(pick);
    row->addWidget(send);
    row->addWidget(remove);
    auto *rowWidget = new QWidget;
    rowWidget->setLayout(row);
    layout.addWidget(rowWidget, {"badge", "chatti", "hochladen", "eigenes"});

    // The queue, for whoever may approve
    auto *queueTitle = new QLabel("<b>Warten auf Freigabe</b>");
    queueTitle->setTextFormat(Qt::RichText);
    auto *queueReload = new QPushButton("Aktualisieren");
    auto *queueHead = new QHBoxLayout;
    queueHead->setContentsMargins(0, 8, 0, 0);
    queueHead->addWidget(queueTitle, 1);
    queueHead->addWidget(queueReload);
    auto *queueHeadWidget = new QWidget;
    queueHeadWidget->setLayout(queueHead);
    queueHeadWidget->hide();
    layout.addWidget(queueHeadWidget, {"badge", "freigabe", "chatti"});
    auto *queue = new QWidget;
    auto *queueList = new QVBoxLayout(queue);
    queueList->setContentsMargins(0, 0, 0, 0);
    queue->hide();
    layout.addWidget(queue, {"badge", "freigabe", "chatti"});

    // The picture picked and made ready, until it is sent
    auto picked = std::make_shared<QByteArray>();

    const auto showPicture = [](QLabel *label, const QString &url) {
        if (url.isEmpty())
        {
            label->clear();
            return;
        }
        QPointer<QLabel> guarded(label);
        webbadges::picture(url, label, [guarded](const QPixmap &picture) {
            if (guarded)
            {
                guarded->setPixmap(picture.scaled(36, 36, Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation));
            }
        });
    };

    // Set further down - the two ask each other, and both live on in the
    // buttons long after this function is done
    auto refreshQueue = std::make_shared<std::function<void()>>();
    const auto refresh = [=]() {
        status->setText("Frage den Badge-Server …");
        chattibadges::me(
            status,
            [=](const chattibadges::Me &me) {
                if (!me.pending.isEmpty())
                {
                    status->setText(
                        "Hochgeladen - wartet auf Freigabe. Bis dahin sehen "
                        "die anderen dein bisheriges Badge, falls du eins "
                        "hast.");
                    showPicture(preview, me.pending);
                }
                else if (!me.approved.isEmpty())
                {
                    status->setText("Dein Badge ist freigegeben und für "
                                    "alle zu sehen.");
                    showPicture(preview, me.approved);
                }
                else
                {
                    status->setText("Noch kein eigenes Badge.");
                    preview->clear();
                }
                remove->setEnabled(!me.pending.isEmpty() ||
                                   !me.approved.isEmpty());
                queueHeadWidget->setVisible(me.isAdmin);
                queue->setVisible(me.isAdmin);
                if (me.isAdmin && *refreshQueue)
                {
                    (*refreshQueue)();
                }
            },
            [status](const QString &error) {
                status->setText(error);
            });
    };

    *refreshQueue = [=]() {
        while (auto *item = queueList->takeAt(0))
        {
            if (auto *widget = item->widget())
            {
                widget->deleteLater();
            }
            delete item;
        }
        chattibadges::waiting(
            queue,
            [=](const std::vector<chattibadges::Waiting> &found) {
                if (found.empty())
                {
                    queueList->addWidget(new QLabel("Nichts wartet."));
                    return;
                }
                for (const auto &entry : found)
                {
                    auto *line = new QWidget;
                    auto *lineLayout = new QHBoxLayout(line);
                    lineLayout->setContentsMargins(0, 0, 0, 0);
                    auto *picture = new QLabel;
                    picture->setFixedSize(40, 40);
                    picture->setAlignment(Qt::AlignCenter);
                    showPicture(picture, entry.image);
                    lineLayout->addWidget(picture);
                    lineLayout->addWidget(new QLabel(entry.login), 1);
                    auto *yes = new QPushButton("Freigeben");
                    auto *no = new QPushButton("Ablehnen");
                    lineLayout->addWidget(yes);
                    lineLayout->addWidget(no);
                    const auto id = entry.userId;
                    const auto settled = [line](const QString &text) {
                        for (auto *button : line->findChildren<QPushButton *>())
                        {
                            button->setEnabled(false);
                        }
                        line->setToolTip(text);
                        line->setEnabled(false);
                    };
                    QObject::connect(yes, &QPushButton::clicked, line,
                                     [id, line, settled, status] {
                                         chattibadges::approve(
                                             id, line,
                                             [settled] {
                                                 settled("Freigegeben");
                                             },
                                             [status](const QString &error) {
                                                 status->setText(error);
                                             });
                                     });
                    QObject::connect(no, &QPushButton::clicked, line,
                                     [id, line, settled, status] {
                                         chattibadges::reject(
                                             id, line,
                                             [settled] {
                                                 settled("Abgelehnt");
                                             },
                                             [status](const QString &error) {
                                                 status->setText(error);
                                             });
                                     });
                    queueList->addWidget(line);
                }
            },
            [queueList](const QString &error) {
                queueList->addWidget(new QLabel(error));
            });
    };

    QObject::connect(pick, &QPushButton::clicked, rowWidget,
                     [rowWidget, preview, status, send, picked] {
                         const auto file = QFileDialog::getOpenFileName(
                             rowWidget, "Bild für dein Badge",
                             QString(),
                             "Bilder (*.png *.jpg *.jpeg *.gif *.webp *.bmp)");
                         if (file.isEmpty())
                         {
                             return;
                         }
                         QImage image(file);
                         if (image.isNull())
                         {
                             status->setText("Das Bild ließ sich nicht "
                                             "öffnen.");
                             return;
                         }
                         *picked = chattibadges::badgePng(image);
                         QPixmap shown;
                         shown.loadFromData(*picked, "PNG");
                         preview->setPixmap(shown.scaled(
                             36, 36, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation));
                         status->setText("So wird es aussehen. Jetzt "
                                         "hochladen.");
                         send->setEnabled(true);
                     });
    QObject::connect(send, &QPushButton::clicked, rowWidget,
                     [=]() {
                         if (picked->isEmpty())
                         {
                             return;
                         }
                         send->setEnabled(false);
                         status->setText("Lade hoch …");
                         chattibadges::upload(
                             *picked, rowWidget,
                             [=]() {
                                 picked->clear();
                                 refresh();
                             },
                             [status, send](const QString &error) {
                                 status->setText(error);
                                 send->setEnabled(true);
                             });
                     });
    QObject::connect(remove, &QPushButton::clicked, rowWidget,
                     [=]() {
                         chattibadges::removeMine(
                             rowWidget,
                             [=]() {
                                 refresh();
                             },
                             [status](const QString &error) {
                                 status->setText(error);
                             });
                     });
    QObject::connect(queueReload, &QPushButton::clicked, queue, [=] {
        (*refreshQueue)();
    });

    if (chattibadges::server().isEmpty())
    {
        status->setText("Erst den Badge-Server oben eintragen.");
    }
    else
    {
        refresh();
    }
    s.chattiBadgesServer.connect(
        [=](const QString &, auto) {
            refresh();
        },
        this->managedConnections_, false);
}

bool ChattiBadgePage::filterElements(const QString &query)
{
    return this->view_->filterElements(query) || query.isEmpty();
}

}  // namespace chatterino
