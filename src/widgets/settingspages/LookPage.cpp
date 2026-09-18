// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/LookPage.hpp"

#include "messages/layouts/AlternateBackground.hpp"
#include "singletons/Settings.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdlib>

namespace chatterino {

namespace {

/// Which of the strengths on offer @a strength is, or is closest to
int strengthIndex(int strength)
{
    const auto &all = alternatebg::STRENGTHS;
    return int(std::min_element(all.begin(), all.end(),
                                [strength](int a, int b) {
                                    return std::abs(a - strength) <
                                           std::abs(b - strength);
                                }) -
               all.begin());
}

}  // namespace

LookPage::LookPage()
    : view(GeneralPageView::withoutNavigation(this))
{
    auto *y = new QVBoxLayout;
    auto *x = new QHBoxLayout;
    x->addWidget(this->view);
    auto *z = new QFrame;
    z->setLayout(x);
    y->addWidget(z);
    this->setLayout(y);

    this->initLayout(*this->view);
}

bool LookPage::filterElements(const QString &query)
{
    if (this->view)
    {
        return this->view->filterElements(query) || query.isEmpty();
    }

    return false;
}

void LookPage::initLayout(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Look");
    layout.addDescription(
        "Classic is Chatterino as it has always looked. Modern rounds tabs "
        "off and gives them a little depth. Switching takes effect right "
        "away.");

    SettingWidget::dropdown("Look", s.uiStyle)->addTo(layout);

    layout.addTitle("Tab bar");
    layout.addDescription(
        "The space around the tabs, behind them. These work with either "
        "look.");

    SettingWidget::colorButton("Background", s.tabBarBackgroundColor)
        ->addTo(layout);
    SettingWidget::checkbox("Gradient", s.tabBarGradient)
        ->setTooltip("Fill the tab bar with a gradient from top to bottom "
                     "instead of a single color.")
        ->addTo(layout);
    SettingWidget::colorButton("Gradient top", s.tabBarGradientTopColor)
        ->conditionallyEnabledBy(s.tabBarGradient)
        ->addTo(layout);
    SettingWidget::colorButton("Gradient bottom", s.tabBarGradientBottomColor)
        ->conditionallyEnabledBy(s.tabBarGradient)
        ->addTo(layout);

    layout.addTitle("Tabs");
    layout.addDescription(
        "The tabs themselves. The selected tab and tabs with new messages or "
        "highlights keep their own colors, so they still stand out.");

    SettingWidget::colorButton("Background", s.tabBackgroundColor)
        ->addTo(layout);
    SettingWidget::colorButton("Selected tab", s.tabSelectedBackgroundColor)
        ->addTo(layout);
    SettingWidget::checkbox("Gradient", s.tabGradient)
        ->setTooltip("Fill tabs with a gradient from top to bottom instead of "
                     "a single color. The selected tab and group headers are "
                     "left flat.")
        ->addTo(layout);
    SettingWidget::colorButton("Gradient top", s.tabGradientTopColor)
        ->conditionallyEnabledBy(s.tabGradient)
        ->addTo(layout);
    SettingWidget::colorButton("Gradient bottom", s.tabGradientBottomColor)
        ->conditionallyEnabledBy(s.tabGradient)
        ->addTo(layout);

    // Back to the theme for the tab bar and the tabs. The gradient colors are
    // kept, so switching a gradient on again brings back what was set up.
    auto *reset = new QPushButton("Use theme colors");
    QObject::connect(reset, &QPushButton::clicked, [&s] {
        s.tabBarBackgroundColor.setValue("");
        s.tabBarGradient.setValue(false);
        s.tabBackgroundColor.setValue("");
        s.tabSelectedBackgroundColor.setValue("");
        s.tabGradient.setValue(false);
    });
    layout.addWidget(reset);

    layout.addTitle("Lesbarkeit");
    layout.addDescription(
        "Jede zweite Nachricht bekommt einen etwas anderen Hintergrund, damit "
        "man die Nachrichten auch im schnellen Chat auseinanderhält. Wie "
        "deutlich und in welcher Farbe, stellst du hier ein; es wirkt sofort.");

    SettingWidget::checkbox("Jede zweite Nachricht absetzen",
                            s.alternateMessages)
        ->setTooltip("Derselbe Schalter wie General -> Messages -> Alternate "
                     "background color.")
        ->addKeywords({"alternate", "background", "Hintergrund"})
        ->addTo(layout);

    auto *strength = layout.addDropdown<int>(
        "Stärke", {"Wie im Theme", "Dezent", "Mittel", "Deutlich", "Stark"},
        s.alternateMessageStrength,
        [](int value) {
            return strengthIndex(value);
        },
        [](const DropdownArgs &args) {
            return alternatebg::STRENGTHS.at(std::clamp(
                args.index, 0, int(alternatebg::STRENGTHS.size()) - 1));
        },
        false,
        "Wie weit sich jede zweite Nachricht abhebt. „Wie im Theme“ ist der "
        "leichte Grauton, den das Theme mitbringt.");

    SettingWidget::colorButton("Farbton", s.alternateMessageTint)
        ->setTooltip("Leer ist neutral: heller im dunklen Theme, dunkler im "
                     "hellen. Mit einer Farbe - etwa einem leichten Violett - "
                     "wird jede zweite Nachricht darin eingefärbt, so stark "
                     "wie oben gewählt.")
        ->conditionallyEnabledBy(s.alternateMessages)
        ->addTo(layout);

    SettingWidget::checkbox("Nur wechseln, wenn jemand anderes schreibt",
                            s.alternateMessagesBySender)
        ->setTooltip(
            "Schreibt jemand mehrere Nachrichten hintereinander, behalten sie "
            "denselben Hintergrund und lesen sich wie ein Block. Der "
            "Hintergrund wechselt erst, wenn jemand anderes schreibt.")
        ->conditionallyEnabledBy(s.alternateMessages)
        ->addTo(layout);

    s.alternateMessages.connect(
        [strength](const bool &on, auto) {
            strength->setEnabled(on);
        },
        this->managedConnections_);

    // Back to the theme's own shade, without a colour
    auto *neutral = new QPushButton("Wie im Theme");
    QObject::connect(neutral, &QPushButton::clicked, [&s] {
        s.alternateMessageStrength.setValue(0);
        s.alternateMessageTint.setValue("");
    });
    layout.addWidget(neutral);

    layout.addStretch();
}

}  // namespace chatterino
