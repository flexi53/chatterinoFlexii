// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/LookPage.hpp"

#include "singletons/Settings.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace chatterino {

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

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
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

    layout.addStretch();
}

}  // namespace chatterino
