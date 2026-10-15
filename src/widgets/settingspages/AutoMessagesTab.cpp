// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/AutoMessagesTab.hpp"

#include "singletons/Settings.hpp"
#include "widgets/settingspages/PageSections.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace chatterino {

using namespace pagesections;

namespace {

using Kind = AutoMessages::Kind;

const std::vector<Kind> KINDS{Kind::Gift, Kind::Sub, Kind::Resub, Kind::Raid,
                              Kind::Chat};

/// One line in the list: on or off, what, where, the first answer
QString titleOf(const AutoMessages::Rule &rule)
{
    QString what = AutoMessages::nameOf(rule.kind);
    if (rule.kind == Kind::Gift)
    {
        what = QStringLiteral("Ab %1 Gift%2")
                   .arg(rule.minCount)
                   .arg(rule.minCount == 1 ? "" : "s");
    }
    else if (rule.kind == Kind::Raid)
    {
        what = QStringLiteral("Raid ab %1").arg(rule.minCount);
    }
    else if (rule.kind == Kind::Chat)
    {
        what = QStringLiteral("Chat: %1").arg(rule.pattern);
    }
    const auto where =
        rule.channel.isEmpty() ? QStringLiteral("alle Kanäle") : rule.channel;
    const auto first = rule.replies.isEmpty() ? QStringLiteral("(keine Antwort)")
                                              : rule.replies.first();
    return QStringLiteral("%1 · %2 → %3").arg(what, where, first);
}

}  // namespace

AutoMessagesTab::AutoMessagesTab()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto &s = *getSettings();

    addText(layout,
            "Dein Account antwortet von selbst im Chat, wenn etwas passiert: "
            "Subs, Resubs, Sub-Gifts ab einer Anzahl, Raids, oder eine "
            "Chat-Nachricht passt auf ein Muster (Regex, wie bei Fossabot).");
    addText(layout,
            "Platzhalter in der Antwort: {user} = wer es war, {anzahl} = "
            "Anzahl Gifts bzw. Raider, {monate} = Sub-Monate, {kanal} = Kanal. "
            "Beispiel: Kussdigga {anzahl}",
            true);
    addText(layout,
            "Es zählt nur, was live hereinkommt - nie der Verlauf beim "
            "Betreten eines Kanals, nie deine eigenen Nachrichten, Subs oder "
            "Gifts. Ein Gift von 10 Subs ist ein Ereignis, nicht zehn. In "
            "einem Kanal gehen höchstens alle 2 Sekunden eine Antwort raus.",
            true);

    this->enabled_ = new QCheckBox("Autonachrichten einschalten");
    this->enabled_->setChecked(s.autoMessagesEnabled);
    QObject::connect(this->enabled_, &QCheckBox::toggled, [](bool on) {
        getSettings()->autoMessagesEnabled = on;
    });
    layout->addWidget(this->enabled_);

    addHeading(layout, "Regeln");
    addText(layout,
            "Bei Gifts und Raids antwortet von mehreren passenden Regeln die "
            "mit der höchsten Schwelle - lege also z. B. ab 1, ab 10 und ab 50 "
            "an. Sonst antwortet die erste passende in der Liste.",
            true);
    this->list_ = new QListWidget;
    this->list_->setMinimumHeight(140);
    layout->addWidget(this->list_);
    {
        auto *row = new QWidget;
        auto *buttons = new QHBoxLayout(row);
        buttons->setContentsMargins(0, 0, 0, 0);
        auto *add = new QPushButton("Neue Regel");
        this->remove_ = new QPushButton("Löschen");
        this->up_ = new QPushButton("Hoch");
        this->down_ = new QPushButton("Runter");
        for (auto *b : {add, this->remove_, this->up_, this->down_})
        {
            buttons->addWidget(b);
        }
        buttons->addStretch(1);
        layout->addWidget(row);

        QObject::connect(add, &QPushButton::clicked, this, [this] {
            AutoMessages::Rule rule;
            rule.replies = {QStringLiteral("Kussdigga {anzahl}")};
            this->rules_.push_back(rule);
            this->current_ = static_cast<int>(this->rules_.size()) - 1;
            this->save();
            this->showList();
        });
        QObject::connect(this->remove_, &QPushButton::clicked, this, [this] {
            if (this->current_ < 0)
            {
                return;
            }
            this->rules_.erase(this->rules_.begin() + this->current_);
            this->current_ = std::min(
                this->current_, static_cast<int>(this->rules_.size()) - 1);
            this->save();
            this->showList();
        });
        const auto move = [this](int by) {
            const auto to = this->current_ + by;
            if (this->current_ < 0 || to < 0 ||
                to >= static_cast<int>(this->rules_.size()))
            {
                return;
            }
            std::swap(this->rules_[this->current_], this->rules_[to]);
            this->current_ = to;
            this->save();
            this->showList();
        };
        QObject::connect(this->up_, &QPushButton::clicked, this, [move] {
            move(-1);
        });
        QObject::connect(this->down_, &QPushButton::clicked, this, [move] {
            move(1);
        });
    }
    QObject::connect(this->list_, &QListWidget::currentRowChanged, this,
                     [this](int row) {
                         if (this->filling_)
                         {
                             return;
                         }
                         this->current_ = row;
                         this->showRule();
                     });
    QObject::connect(this->list_, &QListWidget::itemChanged, this,
                     [this](QListWidgetItem *item) {
                         const auto row = this->list_->row(item);
                         if (this->filling_ || row < 0 ||
                             row >= static_cast<int>(this->rules_.size()))
                         {
                             return;
                         }
                         this->rules_[row].enabled =
                             item->checkState() == Qt::Checked;
                         if (row == this->current_)
                         {
                             const QSignalBlocker b(this->ruleOn_);
                             this->ruleOn_->setChecked(
                                 this->rules_[row].enabled);
                         }
                         this->save();
                     });

    // The chosen rule
    this->editor_ = new QWidget;
    auto *editor = new QVBoxLayout(this->editor_);
    editor->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(this->editor_);
    addHeading(editor, "Regel bearbeiten");

    auto *form = new QFormLayout;
    editor->addLayout(form);

    this->ruleOn_ = new QCheckBox("Regel ist an");
    form->addRow(this->ruleOn_);

    this->kind_ = new QComboBox;
    for (auto kind : KINDS)
    {
        this->kind_->addItem(AutoMessages::nameOf(kind),
                             static_cast<int>(kind));
    }
    form->addRow("Wenn", this->kind_);

    this->channel_ = new QLineEdit;
    this->channel_->setPlaceholderText("leer = alle Kanäle, z. B. zarbex");
    form->addRow("Kanal", this->channel_);

    {
        this->patternRow_ = new QWidget;
        auto *box = new QVBoxLayout(this->patternRow_);
        box->setContentsMargins(0, 0, 0, 0);
        this->pattern_ = new QLineEdit;
        this->pattern_->setPlaceholderText(R"(\bwann\s+stream\b)");
        this->pattern_->setToolTip(
            "Regulärer Ausdruck, Groß-/Kleinschreibung egal. Beispiele:\n"
            "\\bhallo\\b - das Wort hallo\n"
            "^!discord$ - genau !discord\n"
            "(wann|when).*(stream|live) - beides in einem Satz");
        box->addWidget(this->pattern_);
        this->patternStatus_ = new QLabel;
        this->patternStatus_->setWordWrap(true);
        box->addWidget(this->patternStatus_);
        form->addRow("Muster (Regex)", this->patternRow_);
    }
    {
        this->count_ = new QSpinBox;
        this->count_->setRange(1, 100000);
        this->countRow_ = this->count_;
        this->countLabel_ = new QLabel("Ab Anzahl");
        form->addRow(this->countLabel_, this->count_);
    }

    this->replies_ = new QPlainTextEdit;
    this->replies_->setPlaceholderText(
        "Kussdigga {anzahl}\nDanke {user} für {anzahl} Subs!");
    this->replies_->setToolTip(
        "Eine Antwort pro Zeile - jedes Mal wird zufällig eine davon "
        "genommen.");
    this->replies_->setFixedHeight(
        this->replies_->fontMetrics().lineSpacing() * 5 + 16);
    form->addRow("Antworten\n(eine pro Zeile,\nzufällig)", this->replies_);

    {
        auto *row = new QWidget;
        auto *box = new QHBoxLayout(row);
        box->setContentsMargins(0, 0, 0, 0);
        this->delayMin_ = new QDoubleSpinBox;
        this->delayMax_ = new QDoubleSpinBox;
        for (auto *spin : {this->delayMin_, this->delayMax_})
        {
            spin->setRange(0, 600);
            spin->setDecimals(1);
            spin->setSingleStep(0.5);
            spin->setSuffix(" s");
        }
        box->addWidget(new QLabel("zufällig zwischen"));
        box->addWidget(this->delayMin_);
        box->addWidget(new QLabel("und"));
        box->addWidget(this->delayMax_);
        box->addStretch(1);
        row->setToolTip("So lange wartet er vor dem Senden - jedes Mal neu "
                        "gewürfelt, damit es nicht nach Bot aussieht.");
        form->addRow("Verzögerung", row);
    }

    this->cooldown_ = new QSpinBox;
    this->cooldown_->setRange(0, 86400);
    this->cooldown_->setSuffix(" s");
    this->cooldown_->setSpecialValueText("keiner");
    this->cooldown_->setToolTip(
        "So lange antwortet diese Regel im selben Kanal nicht noch einmal.");
    form->addRow("Cooldown", this->cooldown_);

    // Test: what would be said, nothing is sent
    addHeading(editor, "Testen");
    addText(editor, "Zeigt, was gesendet würde - es geht nichts in den Chat.",
            true);
    {
        auto *row = new QWidget;
        auto *box = new QHBoxLayout(row);
        box->setContentsMargins(0, 0, 0, 0);
        this->testText_ = new QLineEdit;
        this->testText_->setPlaceholderText("Test-Nachricht im Chat");
        this->testCount_ = new QSpinBox;
        this->testCount_->setRange(1, 100000);
        this->testCount_->setValue(10);
        this->testCount_->setPrefix("Anzahl ");
        auto *run = new QPushButton("Testen");
        box->addWidget(this->testText_, 1);
        box->addWidget(this->testCount_);
        box->addWidget(run);
        editor->addWidget(row);
        this->testResult_ = new QLabel;
        this->testResult_->setWordWrap(true);
        this->testResult_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        editor->addWidget(this->testResult_);
        QObject::connect(run, &QPushButton::clicked, this, [this] {
            this->showTest();
        });
        QObject::connect(this->testText_, &QLineEdit::returnPressed, this,
                         [this] {
                             this->showTest();
                         });
    }
    layout->addStretch(1);

    // Every field writes back into the chosen rule
    const auto changed = [this] {
        this->takeFields();
    };
    QObject::connect(this->ruleOn_, &QCheckBox::toggled, this, changed);
    QObject::connect(this->kind_, &QComboBox::currentIndexChanged, this,
                     changed);
    QObject::connect(this->channel_, &QLineEdit::textChanged, this, changed);
    QObject::connect(this->pattern_, &QLineEdit::textChanged, this, changed);
    QObject::connect(this->count_, &QSpinBox::valueChanged, this, changed);
    QObject::connect(this->replies_, &QPlainTextEdit::textChanged, this,
                     changed);
    QObject::connect(this->delayMin_, &QDoubleSpinBox::valueChanged, this,
                     changed);
    QObject::connect(this->delayMax_, &QDoubleSpinBox::valueChanged, this,
                     changed);
    QObject::connect(this->cooldown_, &QSpinBox::valueChanged, this, changed);

    this->saveTimer_ = new QTimer(this);
    this->saveTimer_->setSingleShot(true);
    this->saveTimer_->setInterval(500);
    QObject::connect(this->saveTimer_, &QTimer::timeout, this, [this] {
        getSettings()->autoMessagesRules =
            AutoMessages::write(this->rules_);
    });

    this->rules_ = AutoMessages::read(s.autoMessagesRules.getValue());
    this->current_ = this->rules_.empty() ? -1 : 0;
    this->showList();
}

void AutoMessagesTab::showList()
{
    this->filling_ = true;
    this->list_->clear();
    for (const auto &rule : this->rules_)
    {
        auto *item = new QListWidgetItem(titleOf(rule));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked);
        this->list_->addItem(item);
    }
    this->list_->setCurrentRow(this->current_);
    this->filling_ = false;
    this->showRule();
}

void AutoMessagesTab::showRule()
{
    const bool has = this->current_ >= 0 &&
                     this->current_ < static_cast<int>(this->rules_.size());
    this->editor_->setVisible(has);
    this->remove_->setEnabled(has);
    this->up_->setEnabled(has && this->current_ > 0);
    this->down_->setEnabled(
        has && this->current_ < static_cast<int>(this->rules_.size()) - 1);
    if (!has)
    {
        return;
    }

    const auto &rule = this->rules_[this->current_];
    this->filling_ = true;
    this->ruleOn_->setChecked(rule.enabled);
    this->kind_->setCurrentIndex(
        this->kind_->findData(static_cast<int>(rule.kind)));
    this->channel_->setText(rule.channel);
    this->pattern_->setText(rule.pattern);
    this->count_->setValue(rule.minCount);
    this->replies_->setPlainText(rule.replies.join('\n'));
    this->delayMin_->setValue(rule.delayMin);
    this->delayMax_->setValue(rule.delayMax);
    this->cooldown_->setValue(rule.cooldown);
    this->filling_ = false;

    const bool chat = rule.kind == Kind::Chat;
    const bool counted = rule.kind == Kind::Gift || rule.kind == Kind::Raid;
    this->patternRow_->setVisible(chat);
    this->countRow_->setVisible(counted);
    this->countLabel_->setVisible(counted);
    this->countLabel_->setText(rule.kind == Kind::Raid
                                   ? QStringLiteral("Ab Zuschauern")
                                   : QStringLiteral("Ab Gifts"));
    this->testText_->setVisible(chat);
    this->testCount_->setVisible(counted);
    this->testResult_->clear();

    const auto problem =
        chat ? AutoMessages::problemWith(rule.pattern) : QString();
    this->patternStatus_->setText(
        problem.isEmpty()
            ? QStringLiteral("Muster sitzt.")
            : QStringLiteral("<span style=\"color:#ffaa00\">%1</span>")
                  .arg(problem.toHtmlEscaped()));
}

void AutoMessagesTab::takeFields()
{
    if (this->filling_ || this->current_ < 0 ||
        this->current_ >= static_cast<int>(this->rules_.size()))
    {
        return;
    }
    auto &rule = this->rules_[this->current_];
    const auto oldKind = rule.kind;
    rule.enabled = this->ruleOn_->isChecked();
    rule.kind = static_cast<Kind>(this->kind_->currentData().toInt());
    rule.channel = this->channel_->text().trimmed().toLower();
    rule.pattern = this->pattern_->text();
    rule.minCount = this->count_->value();
    rule.replies.clear();
    for (const auto &line : this->replies_->toPlainText().split('\n'))
    {
        if (!line.trimmed().isEmpty())
        {
            rule.replies.append(line.trimmed());
        }
    }
    rule.delayMin = this->delayMin_->value();
    rule.delayMax = this->delayMax_->value();
    rule.cooldown = this->cooldown_->value();

    // The line in the list follows, without rebuilding it (that would take
    // the focus out of the field being typed in)
    if (auto *item = this->list_->item(this->current_))
    {
        const QSignalBlocker b(this->list_);
        item->setText(titleOf(rule));
        item->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked);
    }
    if (rule.kind != oldKind)
    {
        this->showRule();  // other fields belong to another kind
    }
    else if (rule.kind == Kind::Chat)
    {
        const auto problem = AutoMessages::problemWith(rule.pattern);
        this->patternStatus_->setText(
            problem.isEmpty()
                ? QStringLiteral("Muster sitzt.")
                : QStringLiteral("<span style=\"color:#ffaa00\">%1</span>")
                      .arg(problem.toHtmlEscaped()));
    }
    this->save();
}

void AutoMessagesTab::save()
{
    this->saveTimer_->start();
}

void AutoMessagesTab::showTest()
{
    if (this->current_ < 0 ||
        this->current_ >= static_cast<int>(this->rules_.size()))
    {
        return;
    }
    const auto &rule = this->rules_[this->current_];
    AutoMessages::Event event{
        .kind = rule.kind,
        .channel = rule.channel.isEmpty() ? QStringLiteral("zarbex")
                                          : rule.channel,
        .login = QStringLiteral("testuser"),
        .displayName = QStringLiteral("TestUser"),
        .text = this->testText_->text(),
        .count = rule.kind == Kind::Gift || rule.kind == Kind::Raid
                     ? this->testCount_->value()
                     : 1,
        .months = 12,
    };

    // Against every rule, as it would happen - another may win
    const auto index =
        AutoMessages::pick(this->rules_, event, {}, QDateTime::currentDateTimeUtc());
    if (!index)
    {
        this->testResult_->setText(
            "Keine Regel antwortet darauf (aus, anderer Kanal, Muster passt "
            "nicht, Anzahl zu klein oder keine Antwort eingetragen).");
        return;
    }
    const auto &winner = this->rules_[*index];
    QStringList out;
    for (const auto &reply : winner.replies)
    {
        out.append(QStringLiteral("„%1“").arg(
            AutoMessages::fill(reply, event).toHtmlEscaped()));
    }
    this->testResult_->setText(
        QStringLiteral("%1nach %2–%3 s eine davon: %4")
            .arg(static_cast<int>(*index) == this->current_
                     ? QString()
                     : QStringLiteral("Regel %1 gewinnt – ")
                           .arg(*index + 1),
                 QString::number(std::min(winner.delayMin, winner.delayMax),
                                 'f', 1),
                 QString::number(std::max(winner.delayMin, winner.delayMax),
                                 'f', 1),
                 out.join(QStringLiteral(" / "))));
}

}  // namespace chatterino
