// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWindow.hpp"

#include <pajlada/settings/setting.hpp>
#include <QFrame>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace chatterino {

class SettingsPage;
class SettingsDialogTab;
class ModerationPage;
enum class SettingsTabId;

class PageHeader : public QFrame
{
    Q_OBJECT
};

enum class SettingsDialogPreference {
    NoPreference,
    StreamerMode,
    Accounts,
    ModerationActions,
    About,
};

class SettingsDialog : public BaseWindow
{
    SettingsDialog(QWidget *parent);

public:
    static void showDialog(QWidget *parent,
                           SettingsDialogPreference preferredTab =
                               SettingsDialogPreference::NoPreference);

protected:
    void scaleChangedEvent(float newDpi) override;
    void themeChangedEvent() override;
    void showEvent(QShowEvent *) override;

private:
    void refresh();

    void initUi();
    SettingsDialogTab *tab(SettingsTabId id);
    void addTabs();
    /// ChattiFlexii: a quiet heading over a block of pages, so it is clear
    /// which are ours and which are Chatterino's
    QLabel *addSectionLabel(const QString &text);
    /// Hides a heading whose pages are all filtered away
    void refreshHeadings();
    void addTab(std::function<SettingsPage *()> page, const QString &name,
                const QString &iconPath, SettingsTabId id = {},
                Qt::Alignment alignment = Qt::AlignTop);
    void selectTab(SettingsDialogTab *tab, const bool byUser = true);
    void selectTab(SettingsTabId id);
    void filterElements(const QString &query);
    /// ChattiFlexii: shows only the settings that are not where they
    /// started, and the pages that hold them
    void showOnlyChanged(bool only);
    void setElementFilter(const QString &query);
    bool eventFilter(QObject *object, QEvent *event) override;

    void onOkClicked();
    void onCancelClicked();
    void addShortcuts() override;
    void setSearchPlaceholderText();

    struct {
        QWidget *tabContainerContainer{};
        QVBoxLayout *tabContainer{};
        QStackedLayout *pageStack{};
        QPushButton *okButton{};
        QPushButton *cancelButton{};
        QLineEdit *search{};
        /// ChattiFlexii: shows only what was changed
        QCheckBox *onlyChanged{};
    } ui_;
    std::vector<SettingsDialogTab *> tabs_;
    /// ChattiFlexii: how many of them are ours - they come first
    int ownTabs_ = 0;
    QLabel *ownHeading_{};
    QLabel *chatterinoHeading_{};
    SettingsDialogTab *selectedTab_{};
    SettingsDialogTab *lastSelectedByUser_{};
    float dpi_ = 1.0F;

    friend class SettingsDialogTab;
};

}  // namespace chatterino
