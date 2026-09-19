// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/PageSections.hpp"

#include <QAbstractButton>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
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
