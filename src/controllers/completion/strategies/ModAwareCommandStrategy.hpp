// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/completion/strategies/CommandStrategy.hpp"

#include <QString>
#include <QStringList>

namespace chatterino::completion {

/// Suggests commands the way Chatterino does, but leaves out the ones that
/// need a moderator where you are none - in fifty channels half the list
/// would be of no use otherwise. See Eingabefeld.
class ModAwareCommandStrategy : public CommandStrategy
{
public:
    ModAwareCommandStrategy(bool startsWithOnly, bool moderates);

    void apply(const std::vector<CommandItem> &items,
               std::vector<CommandItem> &output,
               const QString &query) const override;

    /// Whether @a name is a command only a moderator can use
    static bool onlyForMods(const QString &name);

private:
    bool moderates_;
};

}  // namespace chatterino::completion
