// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <pajlada/signals/signal.hpp>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>
#include <vector>

class QObject;

/// ChattiFlexii: polls, predictions and channel points, the way the website
/// has them.
///
/// Twitch's public API lets only the broadcaster start a poll or a
/// prediction, and lets nobody vote, predict or redeem a reward. The website
/// does all of it through Twitch's own, internal interface - which takes a
/// login from one of Twitch's own apps. This signs in as the Android TV app
/// with a device code, the way Moltorino and Bajerino (MIT) do, and speaks to
/// that interface. It is separate from the account Chatterino chats with,
/// and only used for what is here.
namespace chatterino::modgql {

using Done = std::function<void()>;
using Failed = std::function<void(const QString &)>;

/// Who is signed in, if anyone
struct Login {
    QString userId;
    QString login;
};

/// What Twitch hands out to start a device login
struct DeviceCode {
    QString deviceCode;
    QString userCode;
    QString verificationUri;
    int intervalSeconds = 5;
};

/// Whether the device login has come through yet
enum class DeviceState {
    Pending,
    SlowDown,
    Done,
    Failed,
};

/// A reward of a channel that can be bought with its points
struct Reward {
    QString id;
    QString title;
    QString prompt;
    int cost = 0;
    QString color;
    QString imageUrl;
    bool enabled = true;
    bool inStock = true;
    bool needsText = false;
};

/// What you have in a channel and can buy there
struct ChannelPoints {
    QString channelId;
    qint64 balance = 0;
    std::vector<Reward> rewards;
};

/// Reads the saved login, if any, and calls @a done once it is known
void load(QObject *receiver, Done done);
/// The account signed in, or nothing
std::optional<Login> currentLogin();
/// Forgets the login
void logout();

/// Loads the saved login and checks it with Twitch once per run, then calls
/// @a done with whether someone is signed in. Later calls answer at once.
void whenReady(QObject *receiver, std::function<void(bool)> done);

/// The login came, went or was found to have run out
pajlada::Signals::NoArgSignal &loginChanged();

/// Someone's balance in a channel became known: the channel's login and the
/// points. Comes with every answer of channelPoints, so whatever shows the
/// balance stays up to date with whatever asked.
pajlada::Signals::Signal<QString, qint64> &balanceChanged();

/// Starts a device login: the user enters the code on Twitch's page
void startDeviceLogin(std::function<void(DeviceCode)> onCode, Failed onError);
/// Asks whether the code from startDeviceLogin has been entered yet
void pollDeviceLogin(const QString &deviceCode,
                     std::function<void(DeviceState, QString)> onState);

/// Checks the saved login is still good, renewing it where it has run out
void checkLogin(std::function<void(std::optional<Login>)> onChecked);

void createPoll(const QString &channelId, const QString &title,
                const QStringList &choices, int durationSeconds,
                std::optional<int> pointsPerVote, Done onDone,
                Failed onError);
void endPoll(const QString &pollId, Done onDone, Failed onError);
/// Votes for @a choiceId with the signed-in account
void voteInPoll(const QString &pollId, const QString &choiceId, Done onDone,
                Failed onError);

void createPrediction(const QString &channelId, const QString &title,
                      const QStringList &outcomes, int windowSeconds,
                      Done onDone, Failed onError);
void lockPrediction(const QString &predictionId, Done onDone, Failed onError);
void cancelPrediction(const QString &predictionId, Done onDone,
                      Failed onError);
void resolvePrediction(const QString &predictionId, const QString &outcomeId,
                       Done onDone, Failed onError);
/// Puts @a points on @a outcomeId
void makePrediction(const QString &predictionId, const QString &outcomeId,
                    int points, Done onDone, Failed onError);

/// Sends the raid the channel with the id @a channelId is setting up now,
/// rather than when the wait is over - or calls it off
void goRaid(const QString &channelId, Done onDone, Failed onError);
void cancelRaid(const QString &channelId, Done onDone, Failed onError);

/// Makes @a targetLogin an editor of the channel with the id @a channelId,
/// or takes that back - only the broadcaster may
void addEditor(const QString &channelId, const QString &targetLogin,
               Done onDone, Failed onError);
void removeEditor(const QString &channelId, const QString &targetLogin,
                  Done onDone, Failed onError);

/// Your balance and the rewards on offer in @a channelLogin
void channelPoints(const QString &channelLogin,
                   std::function<void(ChannelPoints)> onPoints,
                   Failed onError);
/// Buys @a reward, with @a text where it asks for some. Calls back with the
/// balance left.
void redeemReward(const QString &channelId, const Reward &reward,
                  const QString &text, std::function<void(qint64)> onDone,
                  Failed onError);

}  // namespace chatterino::modgql
