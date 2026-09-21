// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/TwitchWebBadges.hpp"

#include "common/Credentials.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"

#include <QJsonArray>
#include <QPointer>
#include <QRegularExpression>
#include <QStringList>

#include <memory>

namespace chatterino::webbadges {

namespace {

/// The website's own id - a login from the browser only works with it. It
/// is the same for everyone and no secret.
constexpr auto WEB_CLIENT_ID = "kimne78kx3ncx6brgo4mv6wki5h1ko";

const QString PROVIDER = QStringLiteral("twitchweb");
const QString NAME = QStringLiteral("browser");

/// What Twitch said went wrong, in one line
QString errorsOf(const QJsonObject &answer)
{
    QStringList messages;
    for (const auto &error : answer.value("errors").toArray())
    {
        messages.append(error.toObject().value("message").toString());
    }
    // Some refusals come without the list, as a message of their own
    if (messages.isEmpty() && answer.contains("message"))
    {
        messages.append(answer.value("message").toString());
    }
    return messages.join(QStringLiteral("; "));
}

QString titlesOf(const QJsonArray &badges)
{
    QStringList titles;
    for (const auto &badge : badges)
    {
        const auto title = badge.toObject().value("title").toString();
        if (!title.isEmpty() && !titles.contains(title))
        {
            titles.append(title);
        }
    }
    return titles.isEmpty() ? QStringLiteral("keine") : titles.join(", ");
}

QString titleOf(const QJsonValue &badge)
{
    const auto title = badge.toObject().value("title").toString();
    return title.isEmpty() ? QStringLiteral("keins") : title;
}

}  // namespace

bool canStore()
{
    return Credentials::isSecure();
}

QString normalize(const QString &pasted)
{
    auto token = pasted.trimmed();
    token.remove('"');
    token.remove('\'');
    if (token.startsWith(QStringLiteral("OAuth "), Qt::CaseInsensitive))
    {
        token = token.mid(6);
    }
    if (token.startsWith(QStringLiteral("auth-token="), Qt::CaseInsensitive))
    {
        token = token.mid(11);
    }
    return token.trimmed();
}

bool looksLikeToken(const QString &token)
{
    static const QRegularExpression shape(
        QStringLiteral(R"(^[A-Za-z0-9]{20,40}$)"));
    return shape.match(token).hasMatch();
}

bool store(const QString &token)
{
    if (!canStore() || !looksLikeToken(token))
    {
        return false;
    }
    Credentials::instance().set(PROVIDER, NAME, token);
    return true;
}

void erase()
{
    Credentials::instance().erase(PROVIDER, NAME);
}

void load(QObject *receiver, std::function<void(const QString &)> done)
{
    if (!canStore())
    {
        done({});
        return;
    }
    Credentials::instance().get(PROVIDER, NAME, receiver, std::move(done));
}

void ask(const QString &token, const QString &query,
         const QJsonObject &variables, QObject *caller,
         std::function<void(const QJsonObject &)> done,
         std::function<void(const QString &)> failed)
{
    QJsonObject body{{"query", query}};
    if (!variables.isEmpty())
    {
        body.insert("variables", variables);
    }

    // Never cached and never logged - the login travels in a header
    NetworkRequest(QUrl(QStringLiteral("https://gql.twitch.tv/gql")),
                   NetworkRequestType::Post)
        .header("Client-Id", WEB_CLIENT_ID)
        .header("Authorization", QStringLiteral("OAuth ") + token)
        .json(body)
        .timeout(15000)
        .caller(caller)
        .onSuccess([done](const NetworkResult &result) {
            done(result.parseJson());
        })
        .onError([failed](const NetworkResult &result) {
            failed(result.formatError());
        })
        .execute();
}

void test(QObject *caller, std::function<void(const QString &report)> done)
{
    const QPointer<QObject> guard(caller);
    load(caller, [guard, done](const QString &token) {
        if (guard.isNull())
        {
            return;
        }
        if (token.isEmpty())
        {
            done(QStringLiteral("Kein Browser-Login gespeichert."));
            return;
        }

        const auto failed = [done](const QString &error) {
            done(QStringLiteral("Twitch war nicht zu erreichen: ") + error);
        };

        // Who it is, the global badges and the one worn - then the same for
        // their own channel, where they always have one of their own
        ask(
            token,
            QStringLiteral("query { currentUser { id login "
                           "selectedBadge { setID version title } "
                           "availableBadges { setID version title } } }"),
            {}, guard.data(),
            [guard, token, done, failed](const QJsonObject &answer) {
                const auto user = answer.value("data")
                                      .toObject()
                                      .value("currentUser")
                                      .toObject();
                if (user.isEmpty())
                {
                    const auto why = errorsOf(answer);
                    done(QStringLiteral(
                             "Twitch nimmt den Login nicht an - abgelaufen "
                             "oder nicht ganz kopiert.") +
                         (why.isEmpty() ? QString() : " (" + why + ")"));
                    return;
                }
                if (guard.isNull())
                {
                    return;
                }

                auto report = std::make_shared<QStringList>();
                report->append(QStringLiteral("Angemeldet als ") +
                               user.value("login").toString() + ".");
                report->append(
                    QStringLiteral("Globale Badges: ") +
                    titlesOf(user.value("availableBadges").toArray()) +
                    QStringLiteral(" - getragen: ") +
                    titleOf(user.value("selectedBadge")) + ".");

                const auto id = user.value("id").toString();
                ask(
                    token,
                    QStringLiteral(
                        "query($id: ID!) { user(id: $id) { self { "
                        "selectedBadge { setID version title } "
                        "availableBadges { setID version title } } } }"),
                    {{"id", id}}, guard.data(),
                    [guard, token, done, failed, report, user,
                     id](const QJsonObject &answer) {
                        const auto self = answer.value("data")
                                              .toObject()
                                              .value("user")
                                              .toObject()
                                              .value("self")
                                              .toObject();
                        report->append(
                            QStringLiteral("Im eigenen Kanal: ") +
                            titlesOf(self.value("availableBadges").toArray()) +
                            QStringLiteral(" - getragen: ") +
                            titleOf(self.value("selectedBadge")) + ".");
                        if (guard.isNull())
                        {
                            return;
                        }

                        // Choosing again what is chosen changes nothing,
                        // but shows whether Twitch lets this app choose
                        const auto global =
                            user.value("selectedBadge").toObject();
                        const auto own = self.value("selectedBadge").toObject();
                        QString query;
                        QJsonObject input;
                        if (!global.isEmpty())
                        {
                            query = QStringLiteral(
                                "mutation($input: SelectGlobalBadgeInput!) { "
                                "selectGlobalBadge(input: $input) { "
                                "user { id } } }");
                            input = {
                                {"badgeSetID", global.value("setID")},
                                {"badgeSetVersion", global.value("version")},
                            };
                        }
                        else if (!own.isEmpty())
                        {
                            query = QStringLiteral(
                                "mutation($input: SelectChannelBadgeInput!) { "
                                "selectChannelBadge(input: $input) { "
                                "user { id } } }");
                            input = {
                                {"channelID", id},
                                {"badgeSetID", own.value("setID")},
                                {"badgeSetVersion", own.value("version")},
                            };
                        }
                        else
                        {
                            report->append(QStringLiteral(
                                "Umstellen: nicht zu prüfen, du trägst "
                                "gerade kein Badge."));
                            done(report->join('\n'));
                            return;
                        }

                        ask(
                            token, query, {{"input", input}}, guard.data(),
                            [done, report](const QJsonObject &answer) {
                                // Chosen is only what comes back with the
                                // user it was chosen for
                                const auto data =
                                    answer.value("data").toObject();
                                const bool chosen =
                                    !data.isEmpty() && !data.begin()
                                                            ->toObject()
                                                            .value("user")
                                                            .toObject()
                                                            .isEmpty();
                                const auto why = errorsOf(answer);
                                if (chosen && why.isEmpty())
                                {
                                    report->append(
                                        QStringLiteral("Umstellen: klappt."));
                                }
                                else
                                {
                                    report->append(
                                        QStringLiteral(
                                            "Umstellen: Twitch lässt es "
                                            "nicht zu - ") +
                                        (why.isEmpty()
                                             ? QStringLiteral("ohne Grund")
                                             : why));
                                }
                                done(report->join('\n'));
                            },
                            failed);
                    },
                    failed);
            },
            failed);
    });
}

}  // namespace chatterino::webbadges
