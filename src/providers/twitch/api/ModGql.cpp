// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/api/ModGql.hpp"

#include "common/Credentials.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "util/Helpers.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QUrlQuery>

#include <algorithm>
#include <utility>
#include <vector>

namespace {

using namespace chatterino;
using namespace chatterino::modgql;

// Twitch's Android TV app - the login is made for it, and its interface
// takes it
constexpr auto TV_CLIENT_ID = "ue6666qo983tsx6so1t0vnawi233wa";
constexpr auto TV_USER_AGENT =
    "Mozilla/5.0 (Linux; Android 7.1; Smart Box C1) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36";
constexpr auto TV_ORIGIN = "https://android.tv.twitch.tv";
constexpr auto TV_REFERER = "https://android.tv.twitch.tv/";
constexpr auto TV_SCOPES = "chat:read channel:moderate "
                           "channel:manage:predictions channel:manage:polls";

// The website - the poll and prediction calls go the way it sends them
constexpr auto WEB_CLIENT_ID = "kimne78kx3ncx6brgo4mv6wki5h1ko";
constexpr auto WEB_CLIENT_VERSION = "ef928475-9403-42f2-8a34-55784bd08e16";
constexpr auto WEB_USER_AGENT =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36";

constexpr auto GQL_URL = "https://gql.twitch.tv/gql";
constexpr int TIMEOUT_MS = 15 * 1000;

// Kept as one secret, so the login and its renewal go together
constexpr auto CREDENTIAL_PROVIDER = "twitchmod";
constexpr auto CREDENTIAL_NAME = "login";

struct Saved {
    QString accessToken;
    QString refreshToken;
    QString userId;
    QString login;
};

Saved &saved()
{
    static Saved instance;
    return instance;
}

void store()
{
    const auto &s = saved();
    if (s.accessToken.isEmpty())
    {
        Credentials::instance().erase(CREDENTIAL_PROVIDER, CREDENTIAL_NAME);
        return;
    }
    QJsonObject root{
        {"access", s.accessToken},
        {"refresh", s.refreshToken},
        {"userId", s.userId},
        {"login", s.login},
    };
    Credentials::instance().set(
        CREDENTIAL_PROVIDER, CREDENTIAL_NAME,
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

QString withoutDashes(QString uuid)
{
    return uuid.remove('{').remove('}').remove('-');
}

const QString &deviceId()
{
    static const QString id = withoutDashes(generateUuid());
    return id;
}

const QString &sessionId()
{
    static const QString id = withoutDashes(generateUuid());
    return id;
}

NetworkRequest tvRequest(const QString &url)
{
    return NetworkRequest(QUrl(url), NetworkRequestType::Post)
        .timeout(TIMEOUT_MS)
        .hideRequestBody()
        .header("Client-Id", TV_CLIENT_ID)
        .header("Accept", "application/json")
        .header("Content-Type", "application/x-www-form-urlencoded")
        .header("Origin", TV_ORIGIN)
        .header("Referer", TV_REFERER)
        .header("User-Agent", TV_USER_AGENT)
        .header("X-Device-Id", deviceId());
}

QJsonObject persisted(const QString &operation, const QString &hash,
                      const QJsonObject &variables)
{
    return {
        {"operationName", operation},
        {"variables", variables},
        {"extensions",
         QJsonObject{{"persistedQuery",
                      QJsonObject{{"version", 1}, {"sha256Hash", hash}}}}},
    };
}

NetworkRequest gqlRequest(const QJsonObject &payload, bool asTv)
{
    auto request =
        NetworkRequest(QUrl(GQL_URL), NetworkRequestType::Post)
            .timeout(TIMEOUT_MS)
            .hideRequestBody()
            .header("Client-Id", asTv ? TV_CLIENT_ID : WEB_CLIENT_ID)
            .header("Client-Session-Id", sessionId())
            .header("Client-Version", WEB_CLIENT_VERSION)
            .header("User-Agent", asTv ? TV_USER_AGENT : WEB_USER_AGENT)
            .header("X-Device-Id", deviceId())
            .header("Authorization", "OAuth " + saved().accessToken)
            .json(QJsonArray{payload});
    if (asTv)
    {
        request = std::move(request)
                      .header("Origin", TV_ORIGIN)
                      .header("Referer", TV_REFERER);
    }
    return request;
}

/// The first answer of a batch, or the answer itself
QJsonObject firstAnswer(const QJsonValue &root)
{
    if (root.isArray())
    {
        return root.toArray().at(0).toObject();
    }
    return root.toObject();
}

/// What went wrong according to the answer, empty when nothing did
QString errorOf(const QJsonObject &answer, const QString &field)
{
    const auto errors = answer.value("errors").toArray();
    if (!errors.isEmpty())
    {
        const auto message = errors.at(0).toObject().value("message").toString();
        return message.isEmpty() ? QStringLiteral("unbekannter Fehler")
                                 : message;
    }
    if (field.isEmpty())
    {
        return {};
    }
    const auto payload =
        answer.value("data").toObject().value(field).toObject();
    const auto error = payload.value("error").toObject();
    if (!error.isEmpty())
    {
        auto message = error.value("message").toString();
        if (message.isEmpty())
        {
            message = error.value("code").toString();
        }
        return message.isEmpty() ? QStringLiteral("unbekannter Fehler")
                                 : message;
    }
    return {};
}

/// Says in German what Twitch said, where it said something we know
QString explain(const QString &error)
{
    if (error.contains("FORBIDDEN", Qt::CaseInsensitive) ||
        error.contains("UNAUTHORIZED", Qt::CaseInsensitive) ||
        error.contains("not authorized", Qt::CaseInsensitive))
    {
        return QStringLiteral(
            "Twitch erlaubt das diesem Account in diesem Kanal nicht.");
    }
    if (error.contains("PersistedQueryNotFound", Qt::CaseInsensitive))
    {
        return QStringLiteral(
            "Twitch hat seine Schnittstelle geändert - das muss in "
            "ChattiFlexii nachgezogen werden.");
    }
    if (error.contains("ACTIVE", Qt::CaseInsensitive) &&
        error.contains("EXISTS", Qt::CaseInsensitive))
    {
        return QStringLiteral("Es läuft schon eine.");
    }
    return error;
}

/// Sends a call that changes something and reports how it went
void mutate(const QJsonObject &payload, const QString &field, bool asTv,
            Done onDone, Failed onError)
{
    if (saved().accessToken.isEmpty())
    {
        onError("Nicht bei Twitch angemeldet.");
        return;
    }
    gqlRequest(payload, asTv)
        .onSuccess([field, onDone, onError](const NetworkResult &result) {
            const auto answer = firstAnswer(result.parseJsonValue());
            const auto error = errorOf(answer, field);
            if (!error.isEmpty())
            {
                qCWarning(chatterinoTwitch)
                    << "Mod GQL" << field << "failed:" << error;
                onError(explain(error));
                return;
            }
            onDone();
        })
        .onError([onError](const NetworkResult &result) {
            onError(result.status() == 401
                        ? QStringLiteral("Die Anmeldung bei Twitch ist "
                                         "abgelaufen.")
                        : "Netzwerkfehler: " + result.formatError());
        })
        .execute();
}

void validate(std::function<void(bool, QString, QString)> onDone)
{
    NetworkRequest(QUrl("https://id.twitch.tv/oauth2/validate"))
        .timeout(TIMEOUT_MS)
        .header("Authorization", "OAuth " + saved().accessToken)
        .onSuccess([onDone](const NetworkResult &result) {
            const auto json = result.parseJson();
            onDone(true, json.value("user_id").toString(),
                   json.value("login").toString());
        })
        .onError([onDone](const NetworkResult &) {
            onDone(false, {}, {});
        })
        .execute();
}

void refresh(std::function<void(bool)> onDone)
{
    if (saved().refreshToken.isEmpty())
    {
        onDone(false);
        return;
    }
    QUrlQuery body;
    body.addQueryItem("client_id", TV_CLIENT_ID);
    body.addQueryItem("grant_type", "refresh_token");
    body.addQueryItem("refresh_token", saved().refreshToken);
    tvRequest("https://id.twitch.tv/oauth2/token")
        .payload(body.toString(QUrl::FullyEncoded).toUtf8())
        .onSuccess([onDone](const NetworkResult &result) {
            const auto json = result.parseJson();
            const auto access = json.value("access_token").toString();
            if (access.isEmpty())
            {
                onDone(false);
                return;
            }
            saved().accessToken = access;
            const auto renewed = json.value("refresh_token").toString();
            if (!renewed.isEmpty())
            {
                saved().refreshToken = renewed;
            }
            store();
            onDone(true);
        })
        .onError([onDone](const NetworkResult &) {
            onDone(false);
        })
        .execute();
}

}  // namespace

namespace chatterino::modgql {

void load(QObject *receiver, Done done)
{
    static bool loaded = false;
    if (loaded)
    {
        done();
        return;
    }
    Credentials::instance().get(
        CREDENTIAL_PROVIDER, CREDENTIAL_NAME, receiver,
        [done](const QString &value) {
            loaded = true;
            const auto root = QJsonDocument::fromJson(value.toUtf8()).object();
            saved() = {
                .accessToken = root.value("access").toString(),
                .refreshToken = root.value("refresh").toString(),
                .userId = root.value("userId").toString(),
                .login = root.value("login").toString(),
            };
            done();
        });
}

std::optional<Login> currentLogin()
{
    const auto &s = saved();
    if (s.accessToken.isEmpty())
    {
        return std::nullopt;
    }
    return Login{.userId = s.userId, .login = s.login};
}

void logout()
{
    const bool hadOne = !saved().accessToken.isEmpty();
    saved() = {};
    store();
    if (hadOne)
    {
        loginChanged().invoke();
    }
}

pajlada::Signals::NoArgSignal &loginChanged()
{
    static pajlada::Signals::NoArgSignal signal;
    return signal;
}

pajlada::Signals::Signal<QString, qint64> &balanceChanged()
{
    static pajlada::Signals::Signal<QString, qint64> signal;
    return signal;
}

void whenReady(QObject *receiver, std::function<void(bool)> done)
{
    // Checked with Twitch once per run; whoever asks while that is under
    // way is answered along with the first
    enum class State {
        Unknown,
        Checking,
        Known,
    };
    static State state = State::Unknown;
    static std::vector<std::pair<QPointer<QObject>, std::function<void(bool)>>>
        waiting;

    if (state == State::Known)
    {
        done(currentLogin().has_value());
        return;
    }
    waiting.emplace_back(receiver, std::move(done));
    if (state == State::Checking)
    {
        return;
    }
    state = State::Checking;

    // Not tied to whoever asked first - if that is gone before the keychain
    // answers, the others still want to know
    static auto *anchor = new QObject;
    load(anchor, [] {
        checkLogin([](const std::optional<Login> &login) {
            state = State::Known;
            auto answered = std::move(waiting);
            waiting.clear();
            for (auto &[who, callback] : answered)
            {
                if (who)
                {
                    callback(login.has_value());
                }
            }
        });
    });
}

void startDeviceLogin(std::function<void(DeviceCode)> onCode, Failed onError)
{
    QUrlQuery body;
    body.addQueryItem("client_id", TV_CLIENT_ID);
    body.addQueryItem("scopes", TV_SCOPES);
    tvRequest("https://id.twitch.tv/oauth2/device")
        .payload(body.toString(QUrl::FullyEncoded).toUtf8())
        .onSuccess([onCode, onError](const NetworkResult &result) {
            const auto json = result.parseJson();
            DeviceCode code{
                .deviceCode = json.value("device_code").toString(),
                .userCode = json.value("user_code").toString(),
                .verificationUri = json.value("verification_uri").toString(),
                .intervalSeconds = std::max(3, json.value("interval").toInt(5)),
            };
            if (code.deviceCode.isEmpty() || code.userCode.isEmpty() ||
                code.verificationUri.isEmpty())
            {
                onError("Twitch hat keinen Code geschickt.");
                return;
            }
            onCode(code);
        })
        .onError([onError](const NetworkResult &result) {
            onError("Twitch hat keinen Code geschickt: " +
                    result.formatError());
        })
        .execute();
}

void pollDeviceLogin(const QString &deviceCode,
                     std::function<void(DeviceState, QString)> onState)
{
    QUrlQuery body;
    body.addQueryItem("client_id", TV_CLIENT_ID);
    body.addQueryItem("device_code", deviceCode);
    body.addQueryItem("grant_type",
                      "urn:ietf:params:oauth:grant-type:device_code");

    auto handle = [onState](const NetworkResult &result) {
        const auto json = result.parseJson();
        const auto access = json.value("access_token").toString();
        if (!access.isEmpty())
        {
            saved() = {
                .accessToken = access,
                .refreshToken = json.value("refresh_token").toString(),
            };
            validate([onState](bool ok, const QString &userId,
                               const QString &login) {
                if (!ok)
                {
                    saved() = {};
                    onState(DeviceState::Failed,
                            "Twitch hat die Anmeldung nicht bestätigt.");
                    return;
                }
                saved().userId = userId;
                saved().login = login;
                store();
                loginChanged().invoke();
                onState(DeviceState::Done, {});
            });
            return;
        }

        // Twitch says it under either name
        auto error = json.value("message").toString();
        if (error.isEmpty())
        {
            error = json.value("error").toString();
        }
        if (error == "authorization_pending")
        {
            onState(DeviceState::Pending, {});
        }
        else if (error == "slow_down")
        {
            onState(DeviceState::SlowDown, {});
        }
        else if (error == "expired_token" || error == "invalid device code")
        {
            onState(DeviceState::Failed,
                    "Der Code ist abgelaufen - fang noch einmal an.");
        }
        else
        {
            onState(DeviceState::Failed,
                    error.isEmpty() ? QStringLiteral("Anmeldung fehlgeschlagen.")
                                    : "Anmeldung fehlgeschlagen: " + error);
        }
    };

    tvRequest("https://id.twitch.tv/oauth2/token")
        .payload(body.toString(QUrl::FullyEncoded).toUtf8())
        .onSuccess(handle)
        .onError(handle)
        .execute();
}

void checkLogin(std::function<void(std::optional<Login>)> onChecked)
{
    if (saved().accessToken.isEmpty())
    {
        onChecked(std::nullopt);
        return;
    }
    validate([onChecked](bool ok, const QString &userId, const QString &login) {
        if (ok)
        {
            saved().userId = userId;
            saved().login = login;
            onChecked(currentLogin());
            return;
        }
        // Run out - renew it once, and forget it if that does not work
        refresh([onChecked](bool renewed) {
            if (!renewed)
            {
                logout();
                onChecked(std::nullopt);
                return;
            }
            validate([onChecked](bool ok, const QString &userId,
                                 const QString &login) {
                if (!ok)
                {
                    logout();
                    onChecked(std::nullopt);
                    return;
                }
                saved().userId = userId;
                saved().login = login;
                store();
                onChecked(currentLogin());
            });
        });
    });
}

void createPoll(const QString &channelId, const QString &title,
                const QStringList &choices, int durationSeconds,
                std::optional<int> pointsPerVote, Done onDone, Failed onError)
{
    QJsonArray choiceArray;
    for (const auto &choice : choices)
    {
        choiceArray.append(QJsonObject{{"title", choice}});
    }
    const QJsonObject input{
        {"title", title},
        {"durationSeconds", durationSeconds},
        {"ownedBy", channelId},
        // Twitch turns the call down when this is on
        {"multichoiceEnabled", false},
        {"isCommunityPointsVotingEnabled", pointsPerVote.has_value()},
        {"communityPointsCost", pointsPerVote.value_or(0)},
        {"choices", choiceArray},
    };
    mutate(persisted("CreatePoll",
                     "4b1461a13fe166a59044961db192747d606f71a89abc3bfdecf79fe8"
                     "62d205cf",
                     {{"input", input}}),
           "createPoll", false, std::move(onDone), std::move(onError));
}

void endPoll(const QString &pollId, Done onDone, Failed onError)
{
    // The website's own way of doing it is turned down - the TV app's works
    mutate(persisted("TerminatePoll",
                     "2701ef0594dae5f532ce68e58cc3036a6d020755eef49927f98c1401"
                     "7fd819b2",
                     {{"input", QJsonObject{{"pollID", pollId}}}}),
           "terminatePoll", true, std::move(onDone), std::move(onError));
}

void createPrediction(const QString &channelId, const QString &title,
                      const QStringList &outcomes, int windowSeconds,
                      Done onDone, Failed onError)
{
    QJsonArray outcomeArray;
    for (qsizetype i = 0; i < outcomes.size(); i++)
    {
        // Blue against pink with two, as on Twitch; blue for all otherwise
        const auto color =
            outcomes.size() == 2 && i == 1 ? "PINK" : "BLUE";
        outcomeArray.append(
            QJsonObject{{"title", outcomes.at(i)}, {"color", color}});
    }
    const QJsonObject input{
        {"channelID", channelId},
        {"title", title},
        {"predictionWindowSeconds", windowSeconds},
        {"outcomes", outcomeArray},
    };
    mutate(persisted("createPredictionEvent",
                     "92268878ac4abe722bcdcba85a4e43acdd7a99d86b05851759e1d8f3"
                     "85cc32ea",
                     {{"input", input}}),
           "createPredictionEvent", false, std::move(onDone),
           std::move(onError));
}

void lockPrediction(const QString &predictionId, Done onDone, Failed onError)
{
    mutate(persisted("LockPrediction",
                     "1f2b1eb44af35f055308e78ffbe81c2f958408f9b32d076a759a84ab"
                     "213285d4",
                     {{"input", QJsonObject{{"id", predictionId}}}}),
           "lockPredictionEvent", false, std::move(onDone),
           std::move(onError));
}

void cancelPrediction(const QString &predictionId, Done onDone, Failed onError)
{
    mutate(persisted("DeletePrediction",
                     "35d375614e426624456ee7be4a2e0fbc0a410c0a91c21f6044cb3cd5"
                     "c38c4e4d",
                     {{"input", QJsonObject{{"id", predictionId}}}}),
           "cancelPredictionEvent", false, std::move(onDone),
           std::move(onError));
}

void resolvePrediction(const QString &predictionId, const QString &outcomeId,
                       Done onDone, Failed onError)
{
    mutate(persisted("ResolvePrediction",
                     "10c803ec11bb8c2957d66bc6a47349dc3c5f51d694585b5ebc37ba65"
                     "6da413c1",
                     {{"input", QJsonObject{{"eventID", predictionId},
                                            {"outcomeID", outcomeId}}}}),
           "resolvePredictionEvent", false, std::move(onDone),
           std::move(onError));
}

void voteInPoll(const QString &pollId, const QString &choiceId, Done onDone,
                Failed onError)
{
    static const auto *mutation = R"(
        mutation VoteInPoll($input: VoteInPollInput!) {
            voteInPoll(input: $input) { error { code } }
        }
    )";
    const QJsonObject input{
        {"pollID", pollId},
        {"choiceID", choiceId},
        {"userID", saved().userId},
        {"voteID", withoutDashes(generateUuid())},
    };
    mutate({{"query", mutation}, {"variables", QJsonObject{{"input", input}}}},
           "voteInPoll", false, std::move(onDone), std::move(onError));
}

void makePrediction(const QString &predictionId, const QString &outcomeId,
                    int points, Done onDone, Failed onError)
{
    const QJsonObject input{
        {"eventID", predictionId},
        {"outcomeID", outcomeId},
        {"points", points},
        {"transactionID", withoutDashes(generateUuid())},
    };
    mutate(persisted("MakePrediction",
                     "b44682ecc88358817009f20e69d75081b1e58825bb40aa53d5dbadcc"
                     "17c881d8",
                     {{"input", input}}),
           "makePrediction", false, std::move(onDone), std::move(onError));
}

void channelPoints(const QString &channelLogin,
                   std::function<void(ChannelPoints)> onPoints,
                   Failed onError)
{
    if (saved().accessToken.isEmpty())
    {
        onError("Nicht bei Twitch angemeldet.");
        return;
    }
    gqlRequest(persisted("ChannelPointsContext",
                         "7fe050e3761eb2cf258d70ee1a21cbd76fa8cf3d7e7b12fc437e"
                         "7029d446b5e3",
                         {{"channelLogin", channelLogin},
                          {"includeGoalTypes", QJsonArray{"CREATOR", "BOOST"}}}),
               true)
        .onSuccess([channelLogin, onPoints,
                    onError](const NetworkResult &result) {
            const auto answer = firstAnswer(result.parseJsonValue());
            const auto error = errorOf(answer, {});
            if (!error.isEmpty())
            {
                onError(explain(error));
                return;
            }
            const auto community =
                answer.value("data").toObject().value("community").toObject();
            const auto channel = community.value("channel").toObject();
            const auto settings =
                channel.value("communityPointsSettings").toObject();
            if (channel.isEmpty() || settings.isEmpty())
            {
                onError("Dieser Kanal hat keine Kanalpunkte.");
                return;
            }

            ChannelPoints points;
            points.channelId = community.value("id").toString();
            points.balance = channel.value("self")
                                 .toObject()
                                 .value("communityPoints")
                                 .toObject()
                                 .value("balance")
                                 .toInteger();
            for (const auto &value : settings.value("customRewards").toArray())
            {
                const auto reward = value.toObject();
                auto image = reward.value("image").toObject();
                if (image.isEmpty())
                {
                    image = reward.value("defaultImage").toObject();
                }
                Reward entry{
                    .id = reward.value("id").toString(),
                    .title = reward.value("title").toString(),
                    .prompt = reward.value("prompt").toString(),
                    .cost = reward.value("cost").toInt(),
                    .color = reward.value("backgroundColor").toString(),
                    .imageUrl = image.value("url2x").toString(
                        image.value("url").toString()),
                    .enabled = reward.value("isEnabled").toBool(true) &&
                               !reward.value("isPaused").toBool(false),
                    .inStock = reward.value("isInStock").toBool(true),
                    .needsText =
                        reward.value("isUserInputRequired").toBool(false),
                };
                if (entry.cost > 0 &&
                    reward.value("pricingType").toString("POINTS") == "POINTS")
                {
                    points.rewards.push_back(std::move(entry));
                }
            }
            std::ranges::sort(points.rewards,
                              [](const Reward &a, const Reward &b) {
                                  return a.cost < b.cost;
                              });
            balanceChanged().invoke(channelLogin.toLower(), points.balance);
            onPoints(std::move(points));
        })
        .onError([onError](const NetworkResult &result) {
            onError("Netzwerkfehler: " + result.formatError());
        })
        .execute();
}

void redeemReward(const QString &channelId, const Reward &reward,
                  const QString &text, std::function<void(qint64)> onDone,
                  Failed onError)
{
    if (saved().accessToken.isEmpty())
    {
        onError("Nicht bei Twitch angemeldet.");
        return;
    }
    QJsonObject input{
        {"channelID", channelId},
        {"cost", reward.cost},
        {"pricingType", "POINTS"},
        {"rewardID", reward.id},
        {"title", reward.title},
        {"transactionID", withoutDashes(generateUuid())},
        {"prompt", reward.prompt.trimmed().isEmpty()
                       ? QJsonValue(QJsonValue::Null)
                       : QJsonValue(reward.prompt)},
    };
    if (!text.trimmed().isEmpty())
    {
        input.insert("textInput", text);
    }
    gqlRequest(persisted("RedeemCustomReward",
                         "d56249a7adb4978898ea3412e196688d4ac3cea1c0c2dfd65561"
                         "d229ea5dcc42",
                         {{"input", input}}),
               true)
        .onSuccess([onDone, onError](const NetworkResult &result) {
            const auto answer = firstAnswer(result.parseJsonValue());
            const auto error =
                errorOf(answer, "redeemCommunityPointsCustomReward");
            if (!error.isEmpty())
            {
                onError(explain(error));
                return;
            }
            onDone(answer.value("data")
                       .toObject()
                       .value("redeemCommunityPointsCustomReward")
                       .toObject()
                       .value("balance")
                       .toInteger(-1));
        })
        .onError([onError](const NetworkResult &result) {
            onError("Netzwerkfehler: " + result.formatError());
        })
        .execute();
}

void addEditor(const QString &channelId, const QString &targetLogin,
               Done onDone, Failed onError)
{
    mutate(persisted("AddEditorUser",
                     "3b52bf904ff9ce1b000ac2358080f538fbd1972c1869804f0d0f345d"
                     "1a56676c",
                     {{"input", QJsonObject{{"channelID", channelId},
                                            {"targetUserLogin", targetLogin}}}}),
           "addEditor", true, std::move(onDone), std::move(onError));
}

void removeEditor(const QString &channelId, const QString &targetLogin,
                  Done onDone, Failed onError)
{
    mutate(persisted("RemoveEditorUser",
                     "4699d38183050854dba547d07e340e72bf1f04578f1037a38a1189fa"
                     "1827790f",
                     {{"input", QJsonObject{{"channelID", channelId},
                                            {"targetUserLogin", targetLogin}}}}),
           "removeEditor", true, std::move(onDone), std::move(onError));
}

void goRaid(const QString &channelId, Done onDone, Failed onError)
{
    mutate(persisted("GoRaid",
                     "878ca88bed0c5a5f0687ad07562cffc0bf6a3136f15e5015c0f5f5f7"
                     "f367f70a",
                     {{"input", QJsonObject{{"sourceID", channelId}}}}),
           "goRaid", false, std::move(onDone), std::move(onError));
}

void cancelRaid(const QString &channelId, Done onDone, Failed onError)
{
    mutate(persisted("CancelRaid",
                     "42a2a699ac85256d72fff2471c75803f7ffbc767ba790725de5ad5d6"
                     "e0163648",
                     {{"input", QJsonObject{{"sourceID", channelId}}}}),
           "cancelRaid", false, std::move(onDone), std::move(onError));
}

}  // namespace chatterino::modgql
