// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/PageSections.hpp"

#include "Application.hpp"
#include "controllers/sound/ISoundController.hpp"
#include "util/RapidJsonSerializeQString.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"

#include <QAbstractButton>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace chatterino::pagesections {

QVBoxLayout *addPageTab(QTabWidget *tabs, const QString &title)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto *content = new QWidget;
    content->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);
    scroll->setWidget(content);

    tabs->addTab(scroll, title);
    return layout;
}

void addHeading(QVBoxLayout *layout, const QString &text)
{
    if (layout->count() > 0)
    {
        layout->addSpacing(12);
    }
    auto *heading = new QLabel(text);
    auto font = heading->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.1);
    heading->setFont(font);
    layout->addWidget(heading);
}

QLabel *addText(QVBoxLayout *layout, const QString &text, bool dimmed)
{
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    if (dimmed)
    {
        label->setEnabled(false);
    }
    layout->addWidget(label);
    return label;
}

QWidget *soundChooser(QWidget *parent,
                      pajlada::Settings::Setting<QString> &setting,
                      pajlada::Signals::SignalHolder &holder)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *choice = new QComboBox;
    auto *listen = new QPushButton("Anhören");
    layout->addWidget(choice);
    layout->addWidget(listen);
    layout->addStretch(1);

    const auto fill = [choice, &setting] {
        const QSignalBlocker blocker(choice);
        choice->clear();
        choice->addItem("Standard-Ping (wie Highlights und Live)", QString());
        for (const auto &[value, name] : ModAlertPopup::builtInSounds())
        {
            choice->addItem(name, value);
        }
        const auto current = setting.getValue();
        if (!current.isEmpty() &&
            !current.startsWith(QStringLiteral("builtin:")))
        {
            choice->addItem(
                QStringLiteral("Eigene: %1").arg(QFileInfo(current).fileName()),
                current);
        }
        choice->addItem("Eigene Datei …", QStringLiteral("__choose__"));
        const auto index = choice->findData(current);
        choice->setCurrentIndex(index >= 0 ? index : 0);
    };
    fill();
    setting.connect(
        [fill](const auto &, auto) {
            fill();
        },
        holder, false);

    QObject::connect(choice, &QComboBox::activated, row,
                     [row, choice, &setting, fill](int index) {
                         const auto value = choice->itemData(index).toString();
                         if (value != QStringLiteral("__choose__"))
                         {
                             setting.setValue(value);
                             return;
                         }
                         const auto file = QFileDialog::getOpenFileName(
                             row, "Ton auswählen", QString(),
                             "Töne (*.wav *.mp3 *.ogg *.flac)");
                         if (file.isEmpty())
                         {
                             fill();
                             return;
                         }
                         setting.setValue(file);
                     });
    QObject::connect(listen, &QPushButton::clicked, row, [&setting] {
        getApp()->getSound()->play(ModAlertPopup::soundUrl(setting.getValue()));
    });
    return row;
}

bool matchesKeywords(const QString &query, const QStringList &keywords)
{
    if (query.isEmpty())
    {
        return true;
    }
    return std::any_of(
        keywords.begin(), keywords.end(), [&query](const QString &keyword) {
            return keyword.contains(query, Qt::CaseInsensitive) ||
                   query.contains(keyword, Qt::CaseInsensitive);
        });
}

bool matchesPageText(const QWidget *page, const QString &query)
{
    if (query.isEmpty())
    {
        return true;
    }

    auto says = [&query](const QString &text) {
        return !text.isEmpty() && text.contains(query, Qt::CaseInsensitive);
    };

    for (const auto *widget : page->findChildren<QWidget *>())
    {
        if (says(widget->toolTip()))
        {
            return true;
        }

        if (const auto *label = qobject_cast<const QLabel *>(widget))
        {
            if (says(label->text()))
            {
                return true;
            }
        }
        else if (const auto *button =
                     qobject_cast<const QAbstractButton *>(widget))
        {
            if (says(button->text()))
            {
                return true;
            }
        }
        else if (const auto *group = qobject_cast<const QGroupBox *>(widget))
        {
            if (says(group->title()))
            {
                return true;
            }
        }
        else if (const auto *tabs = qobject_cast<const QTabWidget *>(widget))
        {
            for (int i = 0; i < tabs->count(); i++)
            {
                if (says(tabs->tabText(i)))
                {
                    return true;
                }
            }
        }
        else if (const auto *list = qobject_cast<const QListWidget *>(widget))
        {
            for (int i = 0; i < list->count(); i++)
            {
                if (says(list->item(i)->text()))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void addButtonRow(QVBoxLayout *layout, QWidget *widget)
{
    auto *row = new QHBoxLayout;
    row->addWidget(widget);
    row->addStretch(1);
    layout->addLayout(row);
}

}  // namespace chatterino::pagesections
