// SPDX-FileCopyrightText: 2020 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/ColorPickerDialog.hpp"

#include "common/Literals.hpp"
#include "providers/colors/NamedColor.hpp"
#include "singletons/Settings.hpp"
#include "providers/colors/ColorProvider.hpp"
#include "widgets/helper/color/AlphaSlider.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/helper/color/ColorInput.hpp"
#include "widgets/helper/color/HueSlider.hpp"
#include "widgets/helper/color/SBCanvas.hpp"

#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>

namespace {

using namespace chatterino;

constexpr size_t COLORS_PER_ROW = 5;
constexpr size_t MAX_RECENT_COLORS = 15;
constexpr size_t MAX_DEFAULT_COLORS = 15;

QGridLayout *makeColorGrid(const auto &items, auto *self,
                           std::size_t maxButtons)
{
    auto *layout = new QGridLayout;

    // TODO(nerix): use std::ranges::views::enumerate (C++ 23)
    for (std::size_t i = 0; auto color : items)
    {
        auto *button = new ColorButton(color);
        button->setMinimumWidth(40);
        QObject::connect(button, &ColorButton::clicked, self, [self, color]() {
            self->setColor(color);
        });

        layout->addWidget(button, static_cast<int>(i / COLORS_PER_ROW),
                          static_cast<int>(i % COLORS_PER_ROW));
        i++;
        if (i >= maxButtons)
        {
            break;
        }
    }
    return layout;
}

/// All color inputs have the same two signals and slots:
/// `colorChanged` and `setColor`.
/// `colorChanged` is emitted when the user changed the color (not after calling `setColor`).
template <typename D, typename W>
void connectSignals(D *dialog, W *widget)
{
    QObject::connect(widget, &W::colorChanged, dialog, &D::setColor);
    QObject::connect(dialog, &D::colorChanged, widget, &W::setColor);
}

}  // namespace

namespace chatterino {

using namespace literals;

ColorPickerDialog::ColorPickerDialog(QColor color, QWidget *parent)
    : BasePopup(
          {
              BaseWindow::EnableCustomFrame,
              BaseWindow::DisableLayoutSave,
              BaseWindow::BoundsCheckOnShow,
          },
          parent)
    , color_(color)
{
    this->setWindowTitle(u"Chatterino - Color picker"_s);
    this->setAttribute(Qt::WA_DeleteOnClose);

    auto *dialogContents = new QHBoxLayout;
    dialogContents->setContentsMargins(10, 10, 10, 10);
    {
        auto *buttons = new QVBoxLayout;

        buttons->addWidget(new QLabel(u"Your colors"_s));
        this->namedColors_ = new QVBoxLayout;
        this->namedColors_->setSpacing(0);
        this->rebuildNamedColors();
        {
            // Kept short so a long list scrolls instead of stretching the
            // dialog past the screen.
            auto *holder = new QWidget;
            holder->setLayout(this->namedColors_);

            auto *scroll = new QScrollArea;
            scroll->setWidget(holder);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setMaximumHeight(120);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            buttons->addWidget(scroll);
        }

        auto *nameColor = new QPushButton(u"Name this color..."_s);
        nameColor->setToolTip(
            u"Give the selected color a name, so you can pick it again by "
            "what you use it for."_s);
        QObject::connect(nameColor, &QPushButton::clicked, this, [this] {
            bool accepted = false;
            auto name = QInputDialog::getText(
                            this, u"Name this color"_s,
                            u"What do you use this color for?"_s,
                            QLineEdit::Normal, QString(), &accepted)
                            .trimmed();
            if (!accepted || name.isEmpty())
            {
                return;
            }

            auto &colors = getSettings()->namedColors;

            // A name already in use takes on the new colour rather than
            // turning up twice
            for (int i = 0; i < static_cast<int>(colors.raw().size()); i++)
            {
                if (colors.raw()[i].name().compare(name,
                                                   Qt::CaseInsensitive) == 0)
                {
                    colors.removeAt(i);
                    colors.insert(NamedColor(name, this->color()), i);
                    this->rebuildNamedColors();
                    return;
                }
            }

            colors.append(NamedColor(name, this->color()));
            this->rebuildNamedColors();
        });
        buttons->addWidget(nameColor);

        buttons->addSpacing(10);

        buttons->addWidget(new QLabel(u"Recently used"_s));
        buttons->addLayout(makeColorGrid(
            ColorProvider::instance().recentColors(), this, MAX_RECENT_COLORS));

        buttons->addSpacing(10);

        buttons->addWidget(new QLabel(u"Default colors"_s));
        buttons->addLayout(
            makeColorGrid(ColorProvider::instance().defaultColors(), this,
                          MAX_DEFAULT_COLORS));

        buttons->addStretch(1);
        buttons->addWidget(new QLabel(u"Selected"_s));
        auto *display = new ColorButton(this->color());
        QObject::connect(this, &ColorPickerDialog::colorChanged, display,
                         &ColorButton::setColor);
        buttons->addWidget(display);

        dialogContents->addLayout(buttons);
        dialogContents->addSpacing(10);
    }

    {
        auto *controls = new QVBoxLayout;

        {
            auto *select = new QVBoxLayout;

            auto *sbCanvas = new SBCanvas(this->color());
            auto *hueSlider = new HueSlider(this->color());
            auto *alphaSlider = new AlphaSlider(this->color());

            connectSignals(this, sbCanvas);
            connectSignals(this, hueSlider);
            connectSignals(this, alphaSlider);

            select->addWidget(sbCanvas, 0, Qt::AlignHCenter);
            select->addWidget(hueSlider);
            select->addWidget(alphaSlider);

            controls->addLayout(select);
        }
        {
            auto *input = new ColorInput(this->color());
            connectSignals(this, input);
            controls->addWidget(input);
        }

        dialogContents->addLayout(controls);
    }

    auto *dialogLayout = new QVBoxLayout(this->getLayoutContainer());
    dialogLayout->addLayout(dialogContents, 1);
    dialogLayout->addStretch(1);

    auto *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, [this] {
        this->colorConfirmed(this->color());
        this->close();
    });
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this,
                     &ColorPickerDialog::close);
    dialogLayout->addWidget(buttonBox, 0, Qt::AlignRight);
}

QColor ColorPickerDialog::color() const
{
    return this->color_;
}

void ColorPickerDialog::setColor(const QColor &color)
{
    if (color == this->color_)
    {
        return;
    }
    this->color_ = color;
    this->colorChanged(color);
}

void ColorPickerDialog::rebuildNamedColors()
{
    while (auto *item = this->namedColors_->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    const auto &colors = getSettings()->namedColors.raw();
    if (colors.empty())
    {
        auto *hint = new QLabel(u"None yet - pick a color and name it."_s);
        hint->setWordWrap(true);
        hint->setEnabled(false);
        this->namedColors_->addWidget(hint);
        return;
    }

    for (int i = 0; i < static_cast<int>(colors.size()); i++)
    {
        const auto entry = colors[i];

        QPixmap swatch(16, 16);
        swatch.fill(entry.color());

        auto *button = new QPushButton(QIcon(swatch), entry.name());
        button->setFlat(true);
        button->setStyleSheet(u"text-align: left; padding: 2px;"_s);
        button->setToolTip(entry.color().name(QColor::HexArgb));
        QObject::connect(button, &QPushButton::clicked, this, [this, entry] {
            this->setColor(entry.color());
        });

        button->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(
            button, &QWidget::customContextMenuRequested, this,
            [this, button, i, entry](const QPoint &pos) {
                QMenu menu(button);

                menu.addAction(u"Rename..."_s, [this, i, entry] {
                    bool accepted = false;
                    auto name = QInputDialog::getText(
                                    this, u"Rename color"_s, u"Name:"_s,
                                    QLineEdit::Normal, entry.name(), &accepted)
                                    .trimmed();
                    if (!accepted || name.isEmpty())
                    {
                        return;
                    }

                    auto &colors = getSettings()->namedColors;
                    colors.removeAt(i);
                    colors.insert(NamedColor(name, entry.color()), i);
                    this->rebuildNamedColors();
                });

                menu.addAction(u"Update to selected color"_s, [this, i, entry] {
                    auto &colors = getSettings()->namedColors;
                    colors.removeAt(i);
                    colors.insert(NamedColor(entry.name(), this->color()), i);
                    this->rebuildNamedColors();
                });

                menu.addSeparator();

                menu.addAction(u"Remove"_s, [this, i] {
                    getSettings()->namedColors.removeAt(i);
                    this->rebuildNamedColors();
                });

                menu.exec(button->mapToGlobal(pos));
            });

        this->namedColors_->addWidget(button);
    }
}

}  // namespace chatterino
