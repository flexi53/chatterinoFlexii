// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ButtonsPage.hpp"

#include "providers/twitch/TwitchWebBadges.hpp"
#include "singletons/Settings.hpp"
#include "widgets/settingspages/HeaderPreview.hpp"
#include "widgets/settingspages/SettingWidget.hpp"
#include "widgets/splits/HeaderParts.hpp"

#include <QAbstractItemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace chatterino {

namespace {

/// A button that puts a section back the way ChattiFlexii starts out
void addStandardButton(GeneralPageView &layout, const QString &tooltip,
                       std::function<void()> reset)
{
    auto *button = new QPushButton("Standard");
    button->setToolTip(tooltip);
    QObject::connect(button, &QPushButton::clicked, std::move(reset));
    layout.addWidget(button);
}

}  // namespace

namespace {

/// Buttons -> Title bar: every part of the header with a tick, in the
/// order they stand. Dragging a row moves the part, as dragging it in the
/// preview does.
class PartList : public QListWidget
{
public:
    PartList()
    {
        this->setDragDropMode(QAbstractItemView::InternalMove);
        this->setDefaultDropAction(Qt::MoveAction);
        this->setSelectionMode(QAbstractItemView::SingleSelection);
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QObject::connect(this, &QListWidget::itemChanged, this,
                         [this](QListWidgetItem *item) {
                             if (this->loading_)
                             {
                                 return;
                             }
                             headerparts::setShown(
                                 partOf(item),
                                 item->checkState() == Qt::Checked);
                         });
        // Read once the move is done, not while the rows are on their way
        QObject::connect(this->model(), &QAbstractItemModel::rowsMoved, this,
                         [this] {
                             this->writeOrderSoon();
                         });
        QObject::connect(this->model(), &QAbstractItemModel::rowsInserted,
                         this, [this] {
                             this->writeOrderSoon();
                         });

        auto *s = getSettings();
        const auto load = [this] {
            this->load();
        };
        s->splitHeaderOrder.connect(load, this->connections_, false);
        s->splitHeaderHidden.connect(load, this->connections_, false);
        s->splitHeaderPictures.connect(load, this->connections_, false);
        s->splitHeaderActivity.connect(load, this->connections_, false);
        this->load();
    }

private:
    static headerparts::Part partOf(const QListWidgetItem *item)
    {
        return static_cast<headerparts::Part>(item->data(Qt::UserRole).toInt());
    }

    void load()
    {
        this->loading_ = true;
        this->clear();
        for (const auto part : headerparts::order())
        {
            const auto &info = headerparts::info(part);
            auto *item = new QListWidgetItem(
                info.canHide ? info.name : info.name + " (bleibt immer)");
            item->setData(Qt::UserRole, static_cast<int>(part));
            item->setToolTip(info.about);
            auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                         Qt::ItemIsDragEnabled;
            if (info.canHide)
            {
                flags |= Qt::ItemIsUserCheckable;
            }
            item->setFlags(flags);
            item->setCheckState(headerparts::isShown(part) ? Qt::Checked
                                                           : Qt::Unchecked);
            this->addItem(item);
        }
        this->setFixedHeight(this->sizeHintForRow(0) * this->count() +
                             2 * this->frameWidth() + 2);
        this->loading_ = false;
    }

    void writeOrderSoon()
    {
        if (this->loading_ || this->writePending_)
        {
            return;
        }
        this->writePending_ = true;
        QTimer::singleShot(0, this, [this] {
            this->writePending_ = false;
            std::vector<headerparts::Part> order;
            for (int i = 0; i < this->count(); i++)
            {
                order.push_back(partOf(this->item(i)));
            }
            headerparts::setOrder(order);
        });
    }

    bool loading_ = false;
    bool writePending_ = false;
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace

ButtonsPage::ButtonsPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    this->tabs_ = new QTabWidget;
    outer->addWidget(this->tabs_);

    this->view_ = GeneralPageView::withoutNavigation(this->tabs_);
    this->tabs_->addTab(this->view_, "Eingabe && Tabs");
    this->titleBar_ = GeneralPageView::withoutNavigation(this->tabs_);
    this->tabs_->addTab(this->titleBar_, "Titelleiste");

    this->initLayout(*this->view_);
    this->initTitleBar(*this->titleBar_);
}

void ButtonsPage::initTitleBar(GeneralPageView &layout)
{
    layout.addTitle("Titelleiste");
    layout.addDescription(
        "Die Leiste über jedem Chat. Zieh in der Vorschau einen Teil an "
        "eine andere Stelle, oder den Rand der Kurve zum Titel hin, um sie "
        "breiter oder schmaler zu machen - ein Doppelklick auf den Rand "
        "stellt sie wieder auf automatisch. Es gilt für alle Chats, sobald "
        "du loslässt.");

    auto *preview = new HeaderPreview;
    layout.addWidget(preview, {"titelleiste", "vorschau", "kurve", "breite",
                               "reihenfolge", "split-kopf", "header"});

    layout.addSubtitle("Was drin ist");
    layout.addDescription(
        "Ein Haken zeigt den Teil, Ziehen einer Zeile verschiebt ihn. Manche "
        "erscheinen nur, wo sie Sinn haben: der Chatmodus, wenn einer an "
        "ist, Moderationsmodus und Chatterliste, wo du Mod bist, das Plus "
        "am letzten Split rechts. In der Vorschau sind sie alle zu sehen.");
    layout.addWidget(new PartList,
                     {"profilbild", "kategorie", "cover", "titel", "kurve",
                      "chatmodus", "moderation", "chatter", "menü", "split"});

    auto &s = *getSettings();
    layout.addSubtitle("Was im Titel steht");
    layout.addDescription(
        "Alles nach dem Namen steht nur da, solange der Kanal live ist - in "
        "dieser Reihenfolge.");
    SettingWidget::checkbox("Name des Kanals", s.headerChannelName)
        ->setTooltip("Weglassen geht nur, solange das Profilbild davor "
                     "steht - fährst du darüber, steht der Name da. Ohne "
                     "Profilbild bleibt der Name, sonst wüsste niemand, "
                     "wessen Chat es ist.")
        ->addKeywords({"name", "kanal", "username", "channel"})
        ->addTo(layout);
    SettingWidget::checkbox("(live) hinter dem Namen", s.headerLiveMarker)
        ->setTooltip("„(live)“, bei einer Wiederholung „(rerun)“. Auch "
                     "ohne es zeigt der rote Punkt am Tab, dass der Kanal "
                     "live ist.")
        ->addKeywords({"live", "rerun"})
        ->addTo(layout);
    SettingWidget::checkbox("Laufzeit (Uptime)", s.headerUptime)
        ->setTooltip("Wie lange der Stream schon läuft, etwa „2h 13m“.")
        ->addKeywords({"uptime", "laufzeit", "dauer"})
        ->addTo(layout);
    SettingWidget::checkbox("Zuschauer", s.headerViewerCount)
        ->setTooltip("Wie viele gerade zuschauen - bei Stream Together "
                     "dazu alle zusammen.")
        ->addKeywords({"zuschauer", "viewer", "user"})
        ->addTo(layout);
    SettingWidget::checkbox("Kategorie", s.headerGame)
        ->setTooltip("Was gestreamt wird, etwa „Just Chatting“.")
        ->addKeywords({"kategorie", "spiel", "game"})
        ->addTo(layout);
    SettingWidget::checkbox("Streamtitel", s.headerStreamTitle)
        ->setTooltip("Der Titel, den der Streamer gesetzt hat. Ist er zu "
                     "lang, endet er mit „…“.")
        ->addKeywords({"titel", "title"})
        ->addTo(layout);

    addStandardButton(layout,
                      "Reihenfolge, Teile, Breite der Kurve und Titel wieder "
                      "so, wie Chatterino die Leiste hat",
                      [] {
                          headerparts::reset();
                      });

    layout.addStretch();
}

void ButtonsPage::initLayout(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Unten in der Eingabezeile");
    layout.addDescription(
        "Die kleine Reihe rechts neben dem Eingabefeld. Was du hier "
        "ausschaltest, ist nicht weg - es steht weiter im Menü des Splits "
        "oder in den Einstellungen.");

    SettingWidget::checkbox("Emotes", s.showEmoteButton)
        ->setTooltip("Öffnet die Liste der Emotes dieses Kanals.")
        ->addKeywords({"emote", "knopf", "button"})
        ->addTo(layout);
    SettingWidget::checkbox("Senden", s.showSendButton)
        ->setTooltip("Schickt die getippte Nachricht ab - dasselbe wie die "
                     "Eingabetaste. Derselbe Schalter wie General -> Show "
                     "send message button.")
        ->addTo(layout);
    SettingWidget::checkbox("Chat leeren", s.showClearChatButton)
        ->setTooltip("Leert diesen Chat hier bei dir, wie „Clear messages“. "
                     "Für alle anderen bleibt alles, wie es ist.")
        ->addKeywords({"clear", "mülleimer", "leeren"})
        ->addTo(layout);
    SettingWidget::checkbox("Fokus-Ansicht", s.showFocusButton)
        ->setTooltip("Blendet Tabs und Knöpfe aus und wieder "
                     "ein - nur in diesem Fenster.")
        ->addKeywords({"fokus", "focus"})
        ->addTo(layout);

    layout.addDescription(
        "Die nächsten beiden erscheinen ohnehin nur in Kanälen, in denen du "
        "Mod oder Streamer bist - sonst könnten sie nichts ausrichten.");
    SettingWidget::checkbox("Mod-Assistent (Schild)", s.showModAssistButton)
        ->setTooltip("Öffnet das Fenster, in dem du für diesen Kanal "
                     "einstellst, worauf der Mod-Assistent achtet.")
        ->addKeywords({"schild", "shield", "mod"})
        ->addTo(layout);
    SettingWidget::checkbox("Alarme stumm (Glocke)", s.showAlertMuteButton)
        ->setTooltip("Schaltet die Alarm-Fenster stumm, ohne etwas an den "
                     "Einstellungen zu ändern. Ist die Glocke aus und die "
                     "Alarme sind gerade stumm, bleiben sie es - schalte sie "
                     "vorher wieder an.")
        ->addKeywords({"glocke", "stumm", "alarm"})
        ->addTo(layout);

    addStandardButton(layout, "Alle Knöpfe wieder so, wie sie am Anfang sind",
                      [&s] {
                          s.showEmoteButton.setValue(
                              s.showEmoteButton.getDefaultValue());
                          s.showSendButton.setValue(
                              s.showSendButton.getDefaultValue());
                          s.showClearChatButton.setValue(
                              s.showClearChatButton.getDefaultValue());
                          s.showFocusButton.setValue(
                              s.showFocusButton.getDefaultValue());
                          s.showModAssistButton.setValue(
                              s.showModAssistButton.getDefaultValue());
                          s.showAlertMuteButton.setValue(
                              s.showAlertMuteButton.getDefaultValue());
                      });

    layout.addTitle("Oben in der Tab-Leiste");
    layout.addDescription(
        "Die Knöpfe links neben den Tabs und das Kreuz am Tab selbst. Das "
        "sind Chatterinos eigene Schalter, hier gleich zur Hand.");
    SettingWidget::inverseCheckbox("Einstellungen (Zahnrad)",
                                   s.hidePreferencesButton)
        ->setTooltip("Ohne ihn kommst du mit ⌘P in die Einstellungen.")
        ->addTo(layout);
    SettingWidget::inverseCheckbox("Eigenes Konto", s.hideUserButton)
        ->setTooltip("Ohne ihn wechselst du das Konto über die "
                     "Einstellungen -> Konten.")
        ->addTo(layout);
    SettingWidget::checkbox("Kreuz zum Schließen am Tab", s.showTabCloseButton)
        ->setTooltip("Ohne es schließt du einen Tab über sein Rechtsklick-"
                     "Menü.")
        ->addTo(layout);

    addStandardButton(layout, "Wieder so, wie Chatterino es zeigt", [&s] {
        s.hidePreferencesButton.setValue(
            s.hidePreferencesButton.getDefaultValue());
        s.hideUserButton.setValue(s.hideUserButton.getDefaultValue());
        s.showTabCloseButton.setValue(
            s.showTabCloseButton.getDefaultValue());
    });

    this->initBadgeTest(layout);

    layout.addStretch();
}

void ButtonsPage::initBadgeTest(GeneralPageView &layout)
{
    layout.addTitle("Badge wechseln (Test)");
    layout.addDescription(
        "Wie im Chat auf twitch.tv wählen, welches Badge du trägst. Twitch "
        "bietet das anderen Apps nicht an - es geht nur mit dem Login deines "
        "Browsers, so wie die Webseite selbst es macht. Das hier prüft erst "
        "einmal, ob Twitch es zulässt; geändert wird dabei nichts.");
    layout.addDescription(
        "So kommst du an den Login: twitch.tv im Browser öffnen, angemeldet "
        "sein, mit Rechtsklick → Untersuchen die Entwicklertools öffnen, "
        "dort unter Speicher bzw. Anwendung → Cookies → twitch.tv den Wert "
        "von „auth-token“ kopieren.");
    layout.addDescription(
        "Der Wert ist wie ein Passwort: Füge ihn nur hier ein - nie in einen "
        "Chat, auf einer Webseite oder bei jemandem, der danach fragt. Er "
        "kommt nur in den Schlüsselbund dieses Computers, nie in die "
        "Einstellungen, einen Export, ein Backup oder den Abgleich.");

    auto *field = new QLineEdit;
    field->setEchoMode(QLineEdit::Password);
    field->setPlaceholderText("auth-token hier einfügen");
    auto *save = new QPushButton("Speichern");
    auto *test = new QPushButton("Testen");
    auto *forget = new QPushButton("Login löschen");

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(field, 1);
    row->addWidget(save);
    row->addWidget(test);
    row->addWidget(forget);
    auto *rowWidget = new QWidget;
    rowWidget->setLayout(row);
    layout.addWidget(rowWidget, {"badge", "login", "browser", "auth-token"});

    auto *status = new QLabel;
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout.addWidget(status, {"badge"});

    if (!webbadges::canStore())
    {
        for (auto *widget : std::initializer_list<QWidget *>{
                 field, save, test, forget})
        {
            widget->setEnabled(false);
        }
        status->setText("Geht nur mit dem Schlüsselbund des Systems - in der "
                        "portablen Version nicht.");
        return;
    }

    const auto showStored = [status] {
        webbadges::load(status, [status](const QString &token) {
            status->setText(token.isEmpty()
                                ? "Kein Browser-Login gespeichert."
                                : "Ein Browser-Login ist im Schlüsselbund.");
        });
    };
    showStored();

    QObject::connect(save, &QPushButton::clicked, status,
                     [field, status] {
                         const auto token =
                             webbadges::normalize(field->text());
                         field->clear();
                         if (!webbadges::store(token))
                         {
                             status->setText(
                                 "Das sieht nicht nach dem auth-token aus - "
                                 "es sind 30 Buchstaben und Ziffern.");
                             return;
                         }
                         status->setText(
                             "Gespeichert im Schlüsselbund. Jetzt Testen.");
                     });
    QObject::connect(test, &QPushButton::clicked, status, [status] {
        status->setText("Frage Twitch …");
        webbadges::test(status, [status](const QString &report) {
            status->setText(report);
        });
    });
    QObject::connect(forget, &QPushButton::clicked, status, [status] {
        webbadges::erase();
        status->setText("Browser-Login gelöscht.");
    });
}

bool ButtonsPage::filterElements(const QString &query)
{
    if (this->view_ == nullptr || this->titleBar_ == nullptr)
    {
        return false;
    }
    const bool buttons = this->view_->filterElements(query);
    const bool titleBar = this->titleBar_->filterElements(query);

    // A search that only finds something about the title bar opens it
    if (!query.isEmpty() && titleBar && !buttons)
    {
        this->tabs_->setCurrentWidget(this->titleBar_);
    }
    else if (!query.isEmpty() && buttons && !titleBar)
    {
        this->tabs_->setCurrentWidget(this->view_);
    }
    return buttons || titleBar || query.isEmpty();
}

}  // namespace chatterino
