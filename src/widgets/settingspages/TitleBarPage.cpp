// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/TitleBarPage.hpp"

#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/settingspages/HeaderPreview.hpp"
#include "widgets/settingspages/SettingWidget.hpp"
#include "widgets/splits/HeaderParts.hpp"

#include <QAbstractItemModel>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
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

/// A colour for one part of the title. Without one it keeps the colour of
/// the title itself, which the button then shows greyed out.
void addTitleColor(GeneralPageView &layout, const headerparts::ItemInfo &info,
                   pajlada::Signals::SignalHolder &holder)
{
    auto *label = new QLabel(info.name + ":");
    auto *button = new ColorButton(QColor());
    button->setFixedSize(50, 24);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *clear = new QPushButton("Standard");
    clear->setToolTip("Nimmt die Farbe wieder weg - der Teil steht dann in "
                      "der Farbe des Titels.");

    auto *row = new QHBoxLayout;
    row->addWidget(label);
    row->addStretch(1);
    row->addWidget(button);
    row->addWidget(clear);
    layout.addLayout(row);

    const auto item = info.item;
    const auto refresh = [button, clear, item] {
        const auto color = headerparts::colorOf(item);
        clear->setEnabled(color.isValid());
        if (color.isValid())
        {
            button->setColor(color);
            button->setGraphicsEffect(nullptr);
            button->setToolTip("Die Farbe, in der dieser Teil im Titel "
                               "steht.");
        }
        else
        {
            button->setColor(getTheme()->messages.textColors.regular);
            auto *faded = new QGraphicsOpacityEffect;
            faded->setOpacity(0.4);
            button->setGraphicsEffect(faded);
            button->setToolTip("Ohne eigene Farbe - der Teil steht in der "
                               "Farbe des Titels.");
        }
    };
    refresh();

    QObject::connect(button, &ColorButton::clicked, [button, item] {
        auto start = headerparts::colorOf(item);
        if (!start.isValid())
        {
            start = getTheme()->messages.textColors.regular;
        }
        auto *dialog = new ColorPickerDialog(start, button);
        QObject::connect(dialog, &ColorPickerDialog::colorConfirmed, button,
                         [item](const QColor &picked) {
                             if (picked.isValid())
                             {
                                 headerparts::setColorOf(item, picked);
                             }
                         });
        dialog->show();
    });
    QObject::connect(clear, &QPushButton::clicked, [item] {
        headerparts::setColorOf(item, QColor());
    });
    getSettings()->headerColors.connect(
        [refresh](const auto &, const auto &) {
            refresh();
        },
        holder, false);
}

}  // namespace

namespace {

/// Titelleiste: every part of the header with a tick, in the
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
        QObject::connect(this->model(), &QAbstractItemModel::rowsInserted, this,
                         [this] {
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

TitleBarPage::TitleBarPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    this->tabs_ = new QTabWidget;
    outer->addWidget(this->tabs_);

    this->titleBar_ = GeneralPageView::withoutNavigation(this->tabs_);
    this->tabs_->addTab(this->titleBar_, "Aufbau");

    this->curve_ = GeneralPageView::withoutNavigation(this->tabs_);
    this->tabs_->addTab(this->curve_, "Kurve");

    this->initTitleBar(*this->titleBar_);
    this->initCurve(*this->curve_);
}

void TitleBarPage::initTitleBar(GeneralPageView &layout)
{
    layout.addTitle("Vorschau");
    layout.addDescription(
        "Die Leiste über jedem Chat, wie sie aussehen wird. Hier kannst du "
        "ziehen: "
        "einen Teil an eine andere Stelle, seinen <b>rechten</b> Rand, um "
        "ihn breiter oder schmaler zu machen, und seinen <b>linken</b>, um "
        "den Abstand zwischen allen Teilen zu ändern. Der Rand der Kurve "
        "zum Titel hin teilt beide auf. Ein Doppelklick auf einen Rand "
        "stellt ihn wieder auf Standard. Es gilt für alle Chats, sobald du "
        "loslässt.");

    auto *preview = new HeaderPreview;
    layout.addWidget(preview,
                     {"titelleiste", "vorschau", "kurve", "breite", "abstand",
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
        "dieser Reihenfolge. In zwei Zeilen steht oben der Kanal mit "
        "Kategorie und Streamtitel, unten die Zahlen.");
    SettingWidget::checkbox("Titelleiste in zwei Zeilen", s.headerTwoRows)
        ->setTooltip("Oben der Kanal, „(live)“, die Kategorie und der "
                     "Streamtitel - unten die Zahlen: Laufzeit, Zuschauer, "
                     "Follows, Leute im Chat und das Tempo. Profilbild, "
                     "Kategoriebild und Kurve stehen über beiden Zeilen. "
                     "Ohne den Haken steht alles in einer Zeile, wie "
                     "Chatterino es zeigt.")
        ->addKeywords(
            {"zwei", "zeilen", "titelleiste", "höhe", "statistik", "zweite"})
        ->addTo(layout);
    SettingWidget::checkbox("Knöpfe in zwei Reihen", s.splitHeaderButtonGrid)
        ->setTooltip("Die kleinen Knöpfe rechts - Tracker, Umfrage, "
                     "angepinnte Nachricht und der Rest - stehen zu zweit "
                     "übereinander statt alle nebeneinander. Das halbiert, "
                     "was sie dem Titel wegnehmen. Wirkt nur mit zwei "
                     "Zeilen; in einer Zeile wäre für zwei Reihen kein "
                     "Platz.")
        ->addKeywords(
            {"knöpfe", "buttons", "reihen", "raster", "grid", "platz"})
        ->addTo(layout);
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
    SettingWidget::checkbox("Zuschauer-Trend", s.headerViewerTrend)
        ->setTooltip("Ein Pfeil hinter der Zuschauerzahl: wie sie sich "
                     "gegenüber der letzten halben Stunde verändert hat, "
                     "etwa „↑ 18 %“. Hält sich der Kanal (weniger als 3 %), "
                     "steht nichts da. Gezählt wird, solange der Tab offen "
                     "ist.")
        ->addKeywords({"trend", "zuschauer", "viewer", "pfeil"})
        ->addTo(layout);
    SettingWidget::checkbox("Follows", s.headerFollowers)
        ->setTooltip("Wie viele dem Kanal folgen. Dieselbe Zahl, die in der "
                     "User-Card steht; alle paar Minuten neu geholt.")
        ->addKeywords({"follower", "follow"})
        ->addTo(layout);
    SettingWidget::checkbox("Leute im Chat", s.headerChatters)
        ->setTooltip("Wie viele gerade im Chat stehen - Twitch sagt das nur "
                     "Moderatoren, deshalb steht es nur in deinen "
                     "Mod-Kanälen.")
        ->addKeywords({"chatter", "im chat", "leute", "zuschauer"})
        ->addTo(layout);
    SettingWidget::checkbox("Nachrichten pro Minute", s.headerMessageRate)
        ->setTooltip("Wie viel gerade geschrieben wird, etwa „42/min“ - "
                     "gezählt aus dem, was hier ankommt, ohne Abfrage bei "
                     "Twitch. Zeigt sich auch, wenn der Kanal nicht live "
                     "ist.")
        ->addKeywords({"nachrichten", "minute", "tempo", "aktivität"})
        ->addTo(layout);
    SettingWidget::checkbox("Tempo-Trend", s.headerMessageRateTrend)
        ->setTooltip("Ein Pfeil hinter dem Tempo: wie die letzte Minute "
                     "gegen die halbe Stunde davor steht, etwa „42/min "
                     "↑ 30 %“. Braucht zehn Minuten offenen Kanal, bevor er "
                     "etwas sagt, und schweigt bei weniger als 3 % "
                     "Unterschied. Er trägt die Farbe von „Zuschauer-Trend“ - "
                     "beide Pfeile sagen dasselbe.")
        ->addKeywords({"tempo", "trend", "pfeil", "nachrichten"})
        ->addTo(layout);

    layout.addTitle("Farben im Titel");
    layout.addDescription(
        "Jeder Teil kann seine eigene Farbe bekommen - etwa die Follows in "
        "Lila oder den Trend in Grün. Ohne eigene Farbe steht ein Teil in "
        "der Farbe des Titels. Das Trennzeichen davor bleibt immer unbunt.");
    for (const auto &info : headerparts::items())
    {
        addTitleColor(layout, info, this->managedConnections_);
    }

    addStandardButton(layout,
                      "Reihenfolge, Teile, Breite der Kurve, Titel und "
                      "Farben wieder so, wie Chatterino die Leiste hat",
                      [] {
                          headerparts::reset();
                      });

    layout.addStretch();
}

void TitleBarPage::initCurve(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Aktivitäts-Kurve");
    layout.addDescription(
        "Die kleine Kurve rechts in der Titelleiste: wie viel im Chat los "
        "war. Ist der Kanal live, reicht sie über den ganzen Stream, sonst "
        "über die letzte Viertelstunde. Wie breit sie ist, ziehst du in der "
        "Vorschau unter „Aufbau“ an ihrem Rand zum Titel hin.");

    SettingWidget::checkbox("Kurve zeigen", s.splitHeaderActivity)
        ->setTooltip("Ohne Haken bleibt die Titelleiste ohne Kurve - Platz "
                     "für Titel und Knöpfe.")
        ->addKeywords({"kurve", "aktivität", "graph"})
        ->addTo(layout);

    layout.addTitle("Was drauf steht");
    SettingWidget::checkbox("Kategorie über dem Abschnitt", s.curveLabels)
        ->setTooltip("Was der Kanal in dem Abschnitt gestreamt hat, mittig "
                     "zwischen den Wechsel-Strichen, mit der Dauer wenn "
                     "Platz ist. Ein zu schmaler Abschnitt bleibt ohne "
                     "Beschriftung - der Tooltip nennt ihn trotzdem.")
        ->addKeywords({"kategorie", "beschriftung", "spiel"})
        ->addTo(layout);
    SettingWidget::checkbox("Klick springt in den Chat", s.curveClick)
        ->setTooltip("Ein Klick auf eine Stelle der Kurve rollt den Chat "
                     "dorthin zurück und lässt die Nachricht aufleuchten. "
                     "Ohne Haken lässt sich der Split wieder überall in der "
                     "Leiste anfassen und verschieben.")
        ->addKeywords({"klick", "springen", "chat"})
        ->addTo(layout);

    layout.addTitle("Marken");
    layout.addDescription(
        "Kleine Dreiecke auf der Zeitachse, damit eine Spitze in der Kurve "
        "einzuordnen ist. Gemerkt wird nur, was ankommt, solange "
        "ChattiFlexii läuft.");
    SettingWidget::checkbox("Erwähnungen (blau)", s.curveMarkMentions)
        ->setTooltip("Wo dein Name gefallen ist.")
        ->addKeywords({"marke", "erwähnung", "ping"})
        ->addTo(layout);
    SettingWidget::checkbox("Alarme (orange)", s.curveMarkAlerts)
        ->setTooltip("Wo ein Alarm-Fenster aufgegangen ist.")
        ->addKeywords({"marke", "alarm"})
        ->addTo(layout);
    SettingWidget::checkbox("Eigene Mod-Aktionen (grün)", s.curveMarkActions)
        ->setTooltip("Wo du selbst getimeoutet oder gebannt hast. In einem "
                     "Kanal, in dem du viel zu tun hast, wird die Achse "
                     "davon schnell voll - deshalb ohne Haken zu Beginn.")
        ->addKeywords({"marke", "timeout", "bann", "moderiert"})
        ->addTo(layout);

    addStandardButton(layout, "Kurve wieder so, wie sie hier ankommt", [&s] {
        s.splitHeaderActivity.setValue(s.splitHeaderActivity.getDefaultValue());
        s.curveLabels.setValue(s.curveLabels.getDefaultValue());
        s.curveClick.setValue(s.curveClick.getDefaultValue());
        s.curveMarkMentions.setValue(s.curveMarkMentions.getDefaultValue());
        s.curveMarkAlerts.setValue(s.curveMarkAlerts.getDefaultValue());
        s.curveMarkActions.setValue(s.curveMarkActions.getDefaultValue());
        s.splitHeaderActivityShare.setValue(
            s.splitHeaderActivityShare.getDefaultValue());
    });

    layout.addStretch();
}

bool TitleBarPage::filterElements(const QString &query)
{
    if (this->titleBar_ == nullptr || this->curve_ == nullptr)
    {
        return false;
    }
    const bool bar = this->titleBar_->filterElements(query);
    const bool curve = this->curve_->filterElements(query);

    // A search that only finds something about the curve opens it
    if (!query.isEmpty() && curve && !bar)
    {
        this->tabs_->setCurrentWidget(this->curve_);
    }
    else if (!query.isEmpty() && bar && !curve)
    {
        this->tabs_->setCurrentWidget(this->titleBar_);
    }
    return bar || curve || query.isEmpty();
}

}  // namespace chatterino
