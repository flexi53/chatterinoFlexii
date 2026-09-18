// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ModHighlightsPage.hpp"

#include "controllers/moderation/ModHighlights.hpp"
#include "providers/twitch/ProfilePictures.hpp"
#include "singletons/Settings.hpp"
#include "util/RoundPixmap.hpp"
#include "util/Twitch.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/settingspages/PageSections.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace chatterino {

using namespace pagesections;

namespace {

/// The size the pictures in the list are loaded and cut at - twice what is
/// shown, so they stay sharp on a Retina screen
constexpr int PICTURE_SIDE = 56;

QString channelText(const QString &login)
{
    const auto count = ModHighlights::instance().modCount(login);
    return QStringLiteral("%1  ·  %2")
        .arg(profilepictures::displayName(login),
             count < 0    ? QStringLiteral("Mods werden geladen")
             : count == 1 ? QStringLiteral("1 Mod")
                          : QStringLiteral("%1 Mods").arg(count));
}

QListWidgetItem *itemFor(QListWidget *list, const QString &login)
{
    for (int i = 0; i < list->count(); i++)
    {
        if (list->item(i)->data(Qt::UserRole).toString() == login)
        {
            return list->item(i);
        }
    }
    return nullptr;
}

std::vector<QString> chosenChannels()
{
    return getSettings()->modHighlightChannels.getValue();
}

bool isChosen(const QString &login)
{
    const auto chosen = chosenChannels();
    return std::find(chosen.begin(), chosen.end(), login) != chosen.end();
}

void setChosen(const QString &login, bool chosen)
{
    auto channels = chosenChannels();
    const auto it = std::find(channels.begin(), channels.end(), login);
    if (chosen && it == channels.end())
    {
        channels.push_back(login);
    }
    else if (!chosen && it != channels.end())
    {
        channels.erase(it);
    }
    else
    {
        return;
    }
    getSettings()->modHighlightChannels.setValue(channels);
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
            this->showChannels();
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
    this->showChannels();
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
    this->search_->setPlaceholderText("Kanal hinzufügen oder suchen …");
    this->search_->setClearButtonEnabled(true);
    layout->addWidget(this->search_);
    addText(layout,
            "Kanalnamen eintippen und Enter drücken - er wird bei "
            "whosthemod.xyz geprüft und hinzugefügt. Ein Haken weg nimmt den "
            "Kanal wieder heraus.",
            true);
    this->listNote_ = addText(layout, QString());
    this->listNote_->hide();

    this->channels_ = new QListWidget;
    this->channels_->setIconSize(QSize(28, 28));
    this->channels_->setMinimumHeight(240);
    this->channels_->setSpacing(2);
    layout->addWidget(this->channels_, 1);

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

    QObject::connect(this->search_, &QLineEdit::textChanged, this, [this] {
        this->showChannels();
    });
    QObject::connect(this->search_, &QLineEdit::returnPressed, this, [this] {
        this->addTypedChannel();
    });
    QObject::connect(refresh, &QPushButton::clicked, this, [this] {
        this->status_->setText("Wird aktualisiert …");
        ModHighlights::instance().refresh();
    });

    // Ticking a channel picks it, unticking lets it go
    QObject::connect(this->channels_, &QListWidget::itemChanged, this,
                     [this](QListWidgetItem *item) {
                         if (this->filling_)
                         {
                             return;
                         }
                         const auto login = item->data(Qt::UserRole).toString();
                         const bool ticked = item->checkState() == Qt::Checked;
                         if (ticked)
                         {
                             this->letGo_.removeAll(login);
                         }
                         else if (!this->letGo_.contains(login))
                         {
                             this->letGo_.append(login);
                         }
                         setChosen(login, ticked);
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
    const auto channels = chosenChannels();
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

void ModHighlightsPage::showChannels()
{
    if (this->channels_ == nullptr)
    {
        return;
    }

    auto filter = this->search_->text().trimmed();
    stripChannelName(filter);
    filter = filter.toLower();

    QStringList shown;
    for (const auto &login : chosenChannels())
    {
        shown.append(login);
    }
    for (const auto &login : this->letGo_)
    {
        if (!shown.contains(login))
        {
            shown.append(login);
        }
    }

    this->filling_ = true;
    this->channels_->clear();
    for (const auto &login : shown)
    {
        if (!filter.isEmpty() && !login.contains(filter))
        {
            continue;
        }

        auto *item = new QListWidgetItem(channelText(login), this->channels_);
        item->setData(Qt::UserRole, login);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(isChosen(login) ? Qt::Checked : Qt::Unchecked);

        // The name as Twitch writes it, and the picture, once they are in
        auto *list = this->channels_;
        profilepictures::whenKnown(login, list,
                                   [list, login](const TwitchProfile &) {
                                       if (auto *found = itemFor(list, login))
                                       {
                                           found->setText(channelText(login));
                                       }
                                   });
        profilepictures::pixmap(
            login, PICTURE_SIDE, list, [list, login](const QPixmap &picture) {
                if (auto *found = itemFor(list, login))
                {
                    found->setIcon(QIcon(roundPixmap(picture, PICTURE_SIDE)));
                }
            });
    }
    this->filling_ = false;
}

void ModHighlightsPage::addTypedChannel()
{
    auto login = this->search_->text().trimmed();
    stripChannelName(login);
    login = login.toLower();
    if (login.isEmpty())
    {
        return;
    }

    this->listNote_->show();
    if (!isValidTwitchLogin(login))
    {
        this->listNote_->setText(
            QStringLiteral("„%1“ ist kein gültiger Kanalname.")
                .arg(login.toHtmlEscaped()));
        return;
    }
    if (isChosen(login))
    {
        this->listNote_->setText(
            QStringLiteral("#%1 ist schon dabei.").arg(login));
        this->search_->clear();
        return;
    }
    if (this->letGo_.contains(login))
    {
        // Let go by mistake a moment ago - its list is still there
        this->letGo_.removeAll(login);
        setChosen(login, true);
        this->listNote_->setText(
            QStringLiteral("#%1 ist wieder dabei.").arg(login));
        this->search_->clear();
        return;
    }

    this->listNote_->setText(QStringLiteral("Prüfe #%1 …").arg(login));
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

            setChosen(login, true);
            this->search_->clear();
            this->listNote_->setText(
                QStringLiteral("#%1 hinzugefügt  ·  %2 Mods")
                    .arg(login)
                    .arg(count));
        });
}

bool ModHighlightsPage::filterElements(const QString &query)
{
    return matchesKeywords(query, {
                                      "mod",
                                      "highlight",
                                      "whosthemod",
                                      "bot",
                                      "kanal",
                                      "farbe",
                                      "profilbild",
                                      "channel",
                                      "plugin",
                                  });
}

}  // namespace chatterino
