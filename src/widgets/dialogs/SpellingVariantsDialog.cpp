// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/SpellingVariantsDialog.hpp"

#include "controllers/highlights/HighlightPhrase.hpp"
#include "providers/colors/ColorProvider.hpp"
#include "singletons/Settings.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace chatterino {

namespace {

QLabel *heading(const QString &text)
{
    auto *label = new QLabel(text);
    auto font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

QCheckBox *option(const QString &text, const QString &tooltip, bool checked)
{
    auto *box = new QCheckBox(text);
    box->setToolTip(tooltip);
    box->setChecked(checked);
    return box;
}

}  // namespace

SpellingVariantsDialog::SpellingVariantsDialog(QWidget *parent)
    : BasePopup(
          {
              BaseWindow::EnableCustomFrame,
              BaseWindow::DisableLayoutSave,
              BaseWindow::BoundsCheckOnShow,
          },
          parent)
{
    this->setWindowTitle(QStringLiteral("Wort mit Schreibweisen"));
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setScaleIndependentSize(560, 480);

    auto *layout = new QVBoxLayout(this->getLayoutContainer());

    auto *intro = new QLabel(QStringLiteral(
        "Gib ein Wort oder einen kurzen Satz ein. Daraus entsteht ein Muster, "
        "das es auch abgewandelt findet - mit Zahlen statt Buchstaben, "
        "gedehnt, mit Punkten dazwischen oder mit Buchstaben aus anderen "
        "Alphabeten, die gleich aussehen. Groß und klein ist egal."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    this->word_ = new QLineEdit;
    this->word_->setPlaceholderText(QStringLiteral("z. B. follower"));
    layout->addWidget(this->word_);

    layout->addWidget(heading(QStringLiteral("Auch finden")));
    const auto defaults = spelling::Options{};
    this->leet_ =
        option(QStringLiteral("Zahlen und Zeichen statt Buchstaben"),
               QStringLiteral("f0ll0w3r, $pam, h4ll0"), defaults.leet);
    this->lookalikes_ = option(
        QStringLiteral("Gleich aussehende Buchstaben"),
        QStringLiteral("fоllower mit kyrillischem о, föllower, fóllower - "
                       "sieht gleich aus, ist aber ein anderes Zeichen"),
        defaults.lookalikes);
    this->stretched_ =
        option(QStringLiteral("Gedehnt oder verkürzt"),
               QStringLiteral("fooollower, folower"), defaults.stretched);
    this->separated_ = option(QStringLiteral("Zeichen zwischen den Buchstaben"),
                              QStringLiteral("f.o.l.l.o.w.e.r, f o l l o w "
                                             "e r, f-o-l-l-o-w-e-r"),
                              defaults.separated);
    this->wholeWord_ = option(
        QStringLiteral("Nur als eigenes Wort"),
        QStringLiteral("An: \"lol\" findet nicht \"lollipop\". Aus: findet es "
                       "auch mitten in längeren Wörtern, etwa \"followers\"."),
        defaults.wholeWord);
    for (auto *box : {this->leet_, this->lookalikes_, this->stretched_,
                      this->separated_, this->wholeWord_})
    {
        layout->addWidget(box);
        QObject::connect(box, &QCheckBox::toggled, this, [this] {
            this->refresh();
        });
    }

    this->examples_ = new QLabel;
    this->examples_->setWordWrap(true);
    this->examples_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(this->examples_);

    layout->addWidget(heading(QStringLiteral("Muster")));
    auto *patternRow = new QHBoxLayout;
    this->pattern_ = new QLineEdit;
    this->pattern_->setReadOnly(true);
    this->pattern_->setFont(
        QFontDatabase::systemFont(QFontDatabase::FixedFont));
    patternRow->addWidget(this->pattern_, 1);
    auto *copy = new QPushButton(QStringLiteral("Kopieren"));
    QObject::connect(copy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(this->pattern_->text());
    });
    patternRow->addWidget(copy);
    layout->addLayout(patternRow);

    layout->addWidget(heading(QStringLiteral("Ausprobieren")));
    this->tryOut_ = new QLineEdit;
    this->tryOut_->setPlaceholderText(QStringLiteral(
        "Eine Nachricht zum Testen, z. B. gratis f0ll0w3r hier"));
    layout->addWidget(this->tryOut_);
    this->result_ = new QLabel;
    this->result_->setWordWrap(true);
    layout->addWidget(this->result_);

    layout->addStretch(1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *cancel = new QPushButton(QStringLiteral("Abbrechen"));
    QObject::connect(cancel, &QPushButton::clicked, this, [this] {
        this->close();
    });
    buttons->addWidget(cancel);
    this->add_ = new QPushButton(QStringLiteral("Als Highlight hinzufügen"));
    QObject::connect(this->add_, &QPushButton::clicked, this, [this] {
        this->addHighlight();
    });
    buttons->addWidget(this->add_);
    layout->addLayout(buttons);

    QObject::connect(this->word_, &QLineEdit::textChanged, this, [this] {
        this->refresh();
    });
    QObject::connect(this->tryOut_, &QLineEdit::textChanged, this, [this] {
        this->refresh();
    });

    this->refresh();
    this->word_->setFocus();
}

spelling::Options SpellingVariantsDialog::options() const
{
    return {
        .leet = this->leet_->isChecked(),
        .lookalikes = this->lookalikes_->isChecked(),
        .stretched = this->stretched_->isChecked(),
        .separated = this->separated_->isChecked(),
        .wholeWord = this->wholeWord_->isChecked(),
    };
}

void SpellingVariantsDialog::refresh()
{
    const auto word = this->word_->text().trimmed();
    const auto options = this->options();
    const auto pattern = spelling::pattern(word, options);
    const auto regex = spelling::compile(pattern);
    const bool usable = !pattern.isEmpty() && regex.isValid();

    this->pattern_->setText(pattern);
    this->pattern_->setCursorPosition(0);
    this->add_->setEnabled(usable);

    QStringList shown;
    for (const auto &example : spelling::examples(word, options))
    {
        shown.append(example.note.isEmpty() ? example.text
                                            : QStringLiteral("%1 (%2)").arg(
                                                  example.text, example.note));
    }
    this->examples_->setText(
        shown.isEmpty()
            ? QString()
            : QStringLiteral("Findet zum Beispiel: %1").arg(shown.join(" · ")));
    this->examples_->setVisible(!shown.isEmpty());

    const auto message = this->tryOut_->text();
    if (word.isEmpty() || message.isEmpty())
    {
        this->result_->clear();
    }
    else if (!usable)
    {
        this->result_->setText(
            QStringLiteral("Aus diesem Wort lässt sich kein Muster bauen."));
        this->result_->setStyleSheet(QStringLiteral("color: #e0a040;"));
    }
    else if (const auto match = regex.match(message); match.hasMatch())
    {
        this->result_->setText(
            QStringLiteral("✓ Gefunden: „%1“").arg(match.captured()));
        this->result_->setStyleSheet(QStringLiteral("color: #4cc26a;"));
    }
    else
    {
        // Say so when it is there, only inside a longer word
        auto inside = options;
        inside.wholeWord = false;
        const bool insideWord =
            options.wholeWord &&
            spelling::compile(spelling::pattern(word, inside))
                .match(message)
                .hasMatch();
        this->result_->setText(
            insideWord ? QStringLiteral("✗ Nicht gefunden - es steckt nur in "
                                        "einem längeren Wort. Dafür „Nur als "
                                        "eigenes Wort“ ausschalten.")
                       : QStringLiteral("✗ Nicht gefunden"));
        this->result_->setStyleSheet(QStringLiteral("color: #9a9a9a;"));
    }
}

void SpellingVariantsDialog::addHighlight()
{
    const auto pattern =
        spelling::pattern(this->word_->text().trimmed(), this->options());
    if (pattern.isEmpty() || !spelling::compile(pattern).isValid())
    {
        return;
    }

    // Shown in mentions like any new highlight, but without flashing the
    // taskbar or a sound - those are a click away in the list
    getSettings()->highlightedMessages.append(HighlightPhrase{
        pattern, true, false, false, true, false, "",
        *ColorProvider::instance().color(ColorType::SelfHighlight)});
    this->close();
}

}  // namespace chatterino
