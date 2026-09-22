// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/WarnDialog.hpp"

#include "singletons/Settings.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace chatterino {

WarnDialog::WarnDialog(const QString &userName, QWidget *parent)
    : QDialog(parent)
{
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setWindowTitle(QStringLiteral("Verwarnen"));
    this->setMinimumWidth(380);

    auto *layout = new QVBoxLayout(this);

    auto *title = new QLabel(QStringLiteral("@%1 verwarnen").arg(userName));
    auto font = title->font();
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);

    auto *about = new QLabel(QStringLiteral(
        "Twitch zeigt die Warnung mit dem Grund an. Weiterschreiben geht "
        "erst, wenn sie bestätigt ist - andere Mods sehen sie auch."));
    about->setWordWrap(true);
    layout->addWidget(about);

    auto *reason = new QComboBox;
    reason->setEditable(true);
    reason->setInsertPolicy(QComboBox::NoInsert);
    reason->addItems(choices(getSettings()->warnReasons.getValue(),
                             getSettings()->warnLastReason.getValue()));
    reason->lineEdit()->setMaxLength(MOST_CHARACTERS);
    reason->lineEdit()->setPlaceholderText(QStringLiteral("Grund"));
    reason->lineEdit()->selectAll();
    layout->addWidget(reason);

    auto *count = new QLabel;
    count->setAlignment(Qt::AlignRight);
    count->setStyleSheet(QStringLiteral("color: #999"));
    layout->addWidget(count);

    auto *buttons = new QDialogButtonBox;
    auto *warn = buttons->addButton(QStringLiteral("Verwarnen"),
                                    QDialogButtonBox::AcceptRole);
    buttons->addButton(QStringLiteral("Abbrechen"),
                       QDialogButtonBox::RejectRole);
    warn->setDefault(true);
    layout->addWidget(buttons);

    // A reason is what Twitch insists on
    const auto refresh = [reason, count, warn] {
        const auto text = reason->currentText().trimmed();
        count->setText(
            QStringLiteral("%1 / %2").arg(text.size()).arg(MOST_CHARACTERS));
        warn->setEnabled(!text.isEmpty());
    };
    QObject::connect(reason, &QComboBox::currentTextChanged, this, refresh);
    refresh();

    QObject::connect(buttons, &QDialogButtonBox::accepted, this,
                     [this, reason] {
                         const auto text = reason->currentText().trimmed();
                         if (text.isEmpty())
                         {
                             return;
                         }
                         getSettings()->warnLastReason.setValue(text);
                         if (this->onWarn)
                         {
                             this->onWarn(text);
                         }
                         this->accept();
                     });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this,
                     &QDialog::reject);

    reason->setFocus();
}

QStringList WarnDialog::choices(const QString &presets, const QString &last)
{
    QStringList offered;
    const auto add = [&offered](const QString &text) {
        const auto trimmed = text.trimmed();
        if (!trimmed.isEmpty() && !offered.contains(trimmed))
        {
            offered.append(trimmed);
        }
    };
    add(last);
    for (const auto &line : presets.split('\n'))
    {
        add(line);
    }
    return offered;
}

}  // namespace chatterino
