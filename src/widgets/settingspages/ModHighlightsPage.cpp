// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ModHighlightsPage.hpp"

#include "controllers/moderation/ModHighlights.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "singletons/Settings.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/settingspages/PageSections.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace chatterino {

using namespace pagesections;

namespace {

/// Channel pictures for the list, by login, cut round
QHash<QString, QPixmap> &channelPictures()
{
    static QHash<QString, QPixmap> pictures;
    return pictures;
}

/// Pictures already on their way, so each is asked for once
QSet<QString> &picturesAsked()
{
    static QSet<QString> asked;
    return asked;
}

QPixmap roundPicture(const QPixmap &source, int side)
{
    QPixmap round(side, side);
    round.fill(Qt::transparent);
    QPainter painter(&round);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(source.scaled(side, side, Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation)));
    painter.drawEllipse(QRectF(0, 0, side, side));
    return round;
}

/// A login as Twitch spells it, from whatever was typed - "#Zarbex" gives
/// "zarbex"
QString typedLogin(const QString &text)
{
    QString login;
    for (const auto c : text.trimmed().toLower())
    {
        if ((c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9') || c == u'_')
        {
            login.append(c);
        }
    }
    return login;
}

QString channelText(const ModHighlightChannel &channel)
{
    QStringList parts{channel.displayName.isEmpty() ? channel.login
                                                    : channel.displayName};
    if (channel.modCount >= 0)
    {
        parts.append(channel.modCount == 1
                         ? QStringLiteral("1 Mod")
                         : QStringLiteral("%1 Mods").arg(channel.modCount));
    }
    if (channel.live)
    {
        parts.append(QStringLiteral("LIVE"));
    }
    return parts.join(QStringLiteral("  ·  "));
}

}  // namespace

ModHighlightsPage::ModHighlightsPage()
{
    ModHighlights::instance().start();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // Above the tabs, since it holds for both
    auto *top = new QWidget;
    auto *topLayout = new QVBoxLayout(top);
    topLayout->setContentsMargins(10, 10, 10, 4);
    topLayout->setSpacing(6);
    addText(topLayout,
            "Markiert Nachrichten von Mods der Kanäle, die du auswählst, mit den "
            "Profilbildern dieser Kanäle - wer bei mehreren davon Mod ist, "
            "bekommt alle ihre Bilder. Die Mod-Listen kommen von whosthemod.xyz "
            "und werden alle sechs Stunden aktualisiert.");
    this->pluginNote_ =
        addText(topLayout,
                "<span style=\"color:#ffaa00\">Braucht das Plugin WhoseTheMod. "
                "Schalte unter Einstellungen → Plugins die Plugins ein und "
                "aktiviere WhoseTheMod, dann ist diese Seite frei.</span>");
    outer->addWidget(top);

    this->tabs_ = new QTabWidget;
    outer->addWidget(this->tabs_, 1);
    this->buildGeneralTab(addPageTab(this->tabs_, "Allgemein"));
    this->buildBotsTab(addPageTab(this->tabs_, "Bots ausschließen"));

    this->managedConnections_.managedConnect(
        ModHighlights::instance().updated, [this] {
            this->showState();
            if (!this->siteListAvailable_)
            {
                this->showChosenChannels();
            }
        });

    // Once the page is up, rather than while the dialog is still being built
    QTimer::singleShot(0, this, [this] {
        this->onShow();
    });
}

void ModHighlightsPage::onShow()
{
    // The plugin may have been switched on or off in the meantime
    this->showState();
    if (!this->asked_ && ModHighlights::pluginAvailable())
    {
        this->asked_ = true;
        this->searchChannels(false);
    }
}

void ModHighlightsPage::buildGeneralTab(QVBoxLayout *layout)
{
    layout->addWidget(this->createCheckBox(
        "Mods der ausgewählten Kanäle markieren",
        getSettings()->modHighlightsEnabled));

    addHeading(layout, "Aussehen");
    layout->addWidget(this->createCheckBox(
        "Nachricht einfärben", getSettings()->modHighlightsColorEnabled,
        "Ohne Farbe bekommt die Nachricht nur die Profilbilder am Ende der "
        "Zeile."));
    {
        auto *button =
            new ColorButton(QColor(getSettings()->modHighlightsColor.getValue()));
        getSettings()->modHighlightsColor.connect(
            [button](const QString &value, auto) {
                button->setColor(QColor(value));
            },
            this->managedConnections_);
        getSettings()->modHighlightsColorEnabled.connect(
            [button](const bool &on, auto) {
                button->setEnabled(on);
            },
            this->managedConnections_);
        QObject::connect(button, &ColorButton::clicked, this, [this] {
            auto *dialog = new ColorPickerDialog(
                QColor(getSettings()->modHighlightsColor.getValue()), this);
            QObject::connect(dialog, &ColorPickerDialog::colorConfirmed, this,
                             [](QColor picked) {
                                 if (picked.isValid())
                                 {
                                     getSettings()->modHighlightsColor.setValue(
                                         picked.name(QColor::HexArgb));
                                 }
                             });
            dialog->show();
        });

        auto *row = new QHBoxLayout;
        row->addWidget(button);
        row->addStretch(1);
        auto *form = new QFormLayout;
        form->addRow("Farbe", row);
        layout->addLayout(form);
    }
    addText(layout,
            "Hat jemand schon ein eigenes User- oder Badge-Highlight, behält "
            "er dessen Farbe und Caption, und die Profilbilder kommen dahinter.",
            true);

    addHeading(layout, "Kanäle");
    this->search_ = new QLineEdit;
    this->search_->setPlaceholderText("Kanal suchen …");
    this->search_->setClearButtonEnabled(true);
    layout->addWidget(this->search_);
    this->listNote_ = addText(layout, QString(), true);
    this->listNote_->hide();

    this->channels_ = new QListWidget;
    this->channels_->setIconSize(QSize(28, 28));
    this->channels_->setMinimumHeight(240);
    this->channels_->setSpacing(2);
    layout->addWidget(this->channels_, 1);

    this->more_ = new QPushButton("Mehr laden");
    this->more_->hide();
    addButtonRow(layout, this->more_);

    auto *statusRow = new QHBoxLayout;
    this->status_ = new QLabel;
    this->status_->setWordWrap(true);
    statusRow->addWidget(this->status_, 1);
    auto *refresh = new QPushButton("Jetzt aktualisieren");
    refresh->setToolTip(
        "Holt die Mod-Listen der ausgewählten Kanäle sofort neu von "
        "whosthemod.xyz.");
    statusRow->addWidget(refresh);
    layout->addLayout(statusRow);

    this->searchDelay_ = new QTimer(this);
    this->searchDelay_->setSingleShot(true);
    this->searchDelay_->setInterval(350);
    QObject::connect(this->searchDelay_, &QTimer::timeout, this, [this] {
        this->searchChannels(false);
    });
    QObject::connect(this->search_, &QLineEdit::textChanged, this, [this] {
        this->searchDelay_->start();
    });
    QObject::connect(this->search_, &QLineEdit::returnPressed, this, [this] {
        this->addTypedChannel();
    });
    QObject::connect(this->more_, &QPushButton::clicked, this, [this] {
        this->searchChannels(true);
    });
    QObject::connect(refresh, &QPushButton::clicked, this, [this] {
        this->status_->setText("Wird aktualisiert …");
        ModHighlights::instance().refresh();
    });

    // Ticking a channel picks it, unticking lets it go
    QObject::connect(
        this->channels_, &QListWidget::itemChanged, this,
        [this](QListWidgetItem *item) {
            if (this->filling_)
            {
                return;
            }
            const auto login = item->data(Qt::UserRole).toString();
            auto channels = getSettings()->modHighlightChannels.getValue();
            const auto it = std::find(channels.begin(), channels.end(), login);
            const bool ticked = item->checkState() == Qt::Checked;
            if (ticked && it == channels.end())
            {
                channels.push_back(login);
            }
            else if (!ticked && it != channels.end())
            {
                channels.erase(it);
            }
            else
            {
                return;
            }
            getSettings()->modHighlightChannels.setValue(channels);
        });
}

void ModHighlightsPage::buildBotsTab(QVBoxLayout *layout)
{
    addText(layout,
            "Bots wie fossabot sind in vielen Kanälen Mod und würden sonst "
            "sämtliche Profilbilder bekommen. Wer hier ausgeschlossen ist, "
            "bekommt weder Profilbilder noch Farbe, egal in welchem Kanal er "
            "Mod ist. Änderungen gelten sofort.");

    addHeading(layout, "Nach Endung");
    layout->addWidget(this->createCheckBox(
        "Alle Namen, die auf „bot“ enden, ausschließen",
        getSettings()->modHighlightsIgnoreBotNames,
        "Fängt auch Bots ab, die nicht in der Liste stehen, etwa sery_bot. "
        "Schalt es aus, falls ein echter Mod so heißt."));

    addHeading(layout, "Diese nie markieren");
    addText(layout, "Namen durch Kommas getrennt.", true);
    {
        auto *ignored = new QPlainTextEdit(
            getSettings()->modHighlightsIgnoredUsers.getValue());
        ignored->setPlaceholderText("fossabot, streamelements, …");
        // Room for the whole list, so it reads without scrolling sideways
        ignored->setFixedHeight(ignored->fontMetrics().lineSpacing() * 6 + 16);

        // Saved once typing pauses, not on every key
        auto *save = new QTimer(ignored);
        save->setSingleShot(true);
        save->setInterval(500);
        QObject::connect(save, &QTimer::timeout, ignored, [ignored] {
            getSettings()->modHighlightsIgnoredUsers.setValue(
                ignored->toPlainText().trimmed());
        });
        QObject::connect(ignored, &QPlainTextEdit::textChanged, save,
                         qOverload<>(&QTimer::start));
        layout->addWidget(ignored);
    }

    layout->addStretch(1);
}

void ModHighlightsPage::showState()
{
    if (this->tabs_ == nullptr || this->status_ == nullptr)
    {
        return;
    }

    const bool available = ModHighlights::pluginAvailable();
    this->pluginNote_->setVisible(!available);
    this->tabs_->setEnabled(available);

    const auto &highlights = ModHighlights::instance();
    const auto channels = getSettings()->modHighlightChannels.getValue();
    if (channels.empty())
    {
        this->status_->setText("Noch kein Kanal ausgewählt.");
        return;
    }

    const auto updated = highlights.lastUpdated();
    this->status_->setText(
        QStringLiteral("%1 %2 ausgewählt  ·  %3 Mods markiert  ·  Stand %4")
            .arg(channels.size())
            .arg(channels.size() == 1 ? QStringLiteral("Kanal")
                                      : QStringLiteral("Kanäle"))
            .arg(highlights.markedCount())
            .arg(updated.isValid()
                     ? QLocale(QLocale::German)
                           .toString(updated.toLocalTime(),
                                     QStringLiteral("dd.MM. HH:mm"))
                     : QStringLiteral("noch nicht geladen")));
}

void ModHighlightsPage::searchChannels(bool more)
{
    const auto query = this->search_->text().trimmed();
    ModHighlights::instance().searchChannels(
        query, more ? this->cursor_ : QString(), this,
        [this, more, query](auto channels, const QString &next) {
            // An answer to a search typed over since is of no use
            if (query != this->search_->text().trimmed())
            {
                return;
            }

            if (!channels)
            {
                this->siteListAvailable_ = false;
                this->cursor_.clear();
                this->more_->hide();
                this->showChosenChannels();
                return;
            }

            this->siteListAvailable_ = true;
            this->cursor_ = next;
            this->more_->setVisible(!next.isEmpty());
            if (!more)
            {
                this->listNote_->setText(
                    "Kein Kanal gefunden. Enter prüft den Namen direkt.");
                this->listNote_->setVisible(channels->empty() &&
                                            !query.isEmpty());
            }
            this->fillChannels(*channels, more, query.isEmpty());
        });
}

void ModHighlightsPage::showChosenChannels()
{
    const auto query = typedLogin(this->search_->text());
    std::vector<ModHighlightChannel> chosen;
    for (const auto &login : getSettings()->modHighlightChannels.getValue())
    {
        if (query.isEmpty() || login.contains(query))
        {
            ModHighlightChannel channel;
            channel.login = login;
            channel.modCount = ModHighlights::instance().modCount(login);
            chosen.push_back(channel);
        }
    }

    this->listNote_->setText(
        "Die Liste aller Kanäle von whosthemod.xyz ist noch nicht online. Bis "
        "dahin: Kanalnamen eintippen und Enter drücken - er wird direkt "
        "geprüft und hinzugefügt.");
    this->listNote_->show();
    this->fillChannels(chosen, false, false);
}

void ModHighlightsPage::fillChannels(
    const std::vector<ModHighlightChannel> &channels, bool append,
    bool chosenFirst)
{
    const auto chosen = getSettings()->modHighlightChannels.getValue();
    const auto isChosen = [&chosen](const QString &login) {
        return std::find(chosen.begin(), chosen.end(), login) != chosen.end();
    };

    // The picked channels lead an unfiltered list, whether or not they are on
    // its first page
    std::vector<ModHighlightChannel> ordered;
    if (chosenFirst)
    {
        for (const auto &login : chosen)
        {
            const auto found =
                std::find_if(channels.begin(), channels.end(),
                             [&login](const ModHighlightChannel &channel) {
                                 return channel.login == login;
                             });
            if (found != channels.end())
            {
                ordered.push_back(*found);
            }
            else
            {
                ModHighlightChannel channel;
                channel.login = login;
                channel.modCount = ModHighlights::instance().modCount(login);
                ordered.push_back(channel);
            }
        }
    }
    for (const auto &channel : channels)
    {
        if (!chosenFirst || !isChosen(channel.login))
        {
            ordered.push_back(channel);
        }
    }

    this->filling_ = true;
    if (!append)
    {
        this->channels_->clear();
    }
    QSet<QString> shown;
    for (int i = 0; i < this->channels_->count(); i++)
    {
        shown.insert(this->channels_->item(i)->data(Qt::UserRole).toString());
    }

    for (const auto &channel : ordered)
    {
        if (shown.contains(channel.login))
        {
            continue;
        }
        shown.insert(channel.login);

        auto *item = new QListWidgetItem(channelText(channel), this->channels_);
        item->setData(Qt::UserRole, channel.login);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(isChosen(channel.login) ? Qt::Checked
                                                     : Qt::Unchecked);
        this->showChannelPicture(channel.login, channel.profileImageUrl);
    }
    this->filling_ = false;
}

void ModHighlightsPage::addTypedChannel()
{
    const auto login = typedLogin(this->search_->text());
    if (login.isEmpty())
    {
        return;
    }

    // A channel the list already shows is ticked right there
    for (int i = 0; i < this->channels_->count(); i++)
    {
        auto *item = this->channels_->item(i);
        if (item->data(Qt::UserRole).toString() == login)
        {
            item->setCheckState(Qt::Checked);
            return;
        }
    }

    this->listNote_->setText(QStringLiteral("Prüfe #%1 …").arg(login));
    this->listNote_->show();
    ModHighlights::instance().checkChannel(
        login, this, [this, login](int count) {
            if (count < 0)
            {
                this->listNote_->setText(
                    QStringLiteral("#%1 konnte nicht geprüft werden - versuch "
                                   "es gleich nochmal.")
                        .arg(login));
                return;
            }
            if (count == 0)
            {
                this->listNote_->setText(
                    QStringLiteral("#%1 hat bei whosthemod keine bekannten "
                                   "Mods.")
                        .arg(login));
                return;
            }

            auto channels = getSettings()->modHighlightChannels.getValue();
            if (std::find(channels.begin(), channels.end(), login) ==
                channels.end())
            {
                channels.push_back(login);
                getSettings()->modHighlightChannels.setValue(channels);
            }
            this->search_->clear();
            this->listNote_->setText(
                QStringLiteral("#%1 hinzugefügt  ·  %2 Mods")
                    .arg(login)
                    .arg(count));
            this->listNote_->show();
            if (!this->siteListAvailable_)
            {
                this->showChosenChannels();
            }
        });
}

void ModHighlightsPage::showChannelPicture(const QString &login,
                                           const QString &url)
{
    const auto applyPicture = [this](const QString &forLogin) {
        const auto picture = channelPictures().value(forLogin);
        if (picture.isNull() || this->channels_ == nullptr)
        {
            return;
        }
        for (int i = 0; i < this->channels_->count(); i++)
        {
            auto *item = this->channels_->item(i);
            if (item->data(Qt::UserRole).toString() == forLogin)
            {
                item->setIcon(QIcon(picture));
            }
        }
    };

    if (channelPictures().contains(login))
    {
        applyPicture(login);
        return;
    }
    const auto key = login + u'\n' + url;
    if (picturesAsked().contains(key))
    {
        return;
    }
    picturesAsked().insert(key);

    if (url.isEmpty())
    {
        // Not in the site's answer - Twitch has it
        QPointer<ModHighlightsPage> self(this);
        getHelix()->fetchUsers(
            {}, {login},
            [self, login](const std::vector<HelixUser> &users) {
                if (!self || users.empty() ||
                    users.front().profileImageUrl.isEmpty())
                {
                    return;
                }
                auto picture = users.front().profileImageUrl;
                self->showChannelPicture(
                    login, picture.replace(QStringLiteral("300x300"),
                                           QStringLiteral("70x70")));
            },
            [] {});
        return;
    }

    static auto *manager = new QNetworkAccessManager;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "Chatterino");
    auto *reply = manager->get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply,
                     &QObject::deleteLater);
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [reply, login, applyPicture] {
                         QPixmap pixmap;
                         if (reply->error() != QNetworkReply::NoError ||
                             !pixmap.loadFromData(reply->readAll()))
                         {
                             return;
                         }
                         channelPictures().insert(login,
                                                  roundPicture(pixmap, 56));
                         applyPicture(login);
                     });
}

bool ModHighlightsPage::filterElements(const QString &query)
{
    static const QStringList keywords{
        "mod",   "highlight", "whosthemod", "bot",   "kanal",
        "farbe", "profilbild", "channel",   "plugin",
    };

    if (query.isEmpty())
    {
        return true;
    }
    for (const auto &keyword : keywords)
    {
        if (keyword.contains(query, Qt::CaseInsensitive) ||
            query.contains(keyword, Qt::CaseInsensitive))
        {
            return true;
        }
    }
    return false;
}

}  // namespace chatterino
