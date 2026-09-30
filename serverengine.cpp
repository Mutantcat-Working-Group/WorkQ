#include "serverengine.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QUuid>
#include <QDateTime>
#include <QSet>
#include <chrono>

namespace {
QJsonObject objectAt(const QJsonArray &args, int index)
{
    if (index < 0 || index >= args.size() || !args.at(index).isObject())
        return QJsonObject();
    return args.at(index).toObject();
}

QString stringAt(const QJsonArray &args, int index)
{
    if (index < 0 || index >= args.size())
        return QString();
    return args.at(index).toString();
}

QString jsonString(const QJsonObject &obj, const char *key)
{
    return obj.value(QLatin1String(key)).toString();
}

long jsonLong(const QJsonObject &obj, const char *key)
{
    return obj.value(QLatin1String(key)).toVariant().toLongLong();
}
}

ServerEngine::ServerEngine(QObject *parent)
    : QObject(parent)
{
    connect(&mSocket, &QWebSocket::connected, this, &ServerEngine::onWebSocketConnected);
    connect(&mSocket, &QWebSocket::disconnected, this, &ServerEngine::onWebSocketDisconnected);
    connect(&mSocket, &QWebSocket::textMessageReceived, this, &ServerEngine::onTextMessageReceived);
    connect(&mSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onWebSocketError(QAbstractSocket::SocketError)));

    mReconnectTimer.setSingleShot(true);
    connect(&mReconnectTimer, &QTimer::timeout, this, &ServerEngine::tryReconnect);

    mTypingClearTimer.setSingleShot(true);
    connect(&mTypingClearTimer, &QTimer::timeout, this, [this]() {
        if (!mTypingChannel.isEmpty() && !mTypingUser.isEmpty()) {
            auto it = mChannels.find(mTypingChannel);
            if (it != mChannels.end()) {
                it->typing.clear();
                it->fellow->setTyping("");
                updateChannel(mTypingChannel);
            }
            mTypingChannel.clear();
            mTypingUser.clear();
        }
    });
}

ServerEngine::~ServerEngine()
{
    stop();
}

void ServerEngine::setView(IFeiqView *view)
{
    mView = view;
}

void ServerEngine::start(const QString &url, const QString &username, const QString &password)
{
    stop();

    mBaseUrl = url.trimmed();
    while (mBaseUrl.endsWith('/'))
        mBaseUrl.chop(1);

    mUsername = username.trimmed();
    mPassword = password;
    mEnabled = !mBaseUrl.isEmpty() && !mUsername.isEmpty();
    mExpectStop = false;

    if (!mEnabled)
    {
        emit stateChanged(State::Disconnected, "未配置服务器");
        return;
    }

    const QUrl parsed(mBaseUrl);
    if (parsed.scheme() != QLatin1String("http") && parsed.scheme() != QLatin1String("https"))
    {
        emit stateChanged(State::Disconnected, "服务器地址需以 http:// 或 https:// 开头");
        return;
    }

    emit stateChanged(State::Connecting, "登录服务器...");
    login();
}

void ServerEngine::stop()
{
    mExpectStop = true;
    mReconnectTimer.stop();
    disconnectHub();
    mPendingClientIds.clear();
    mSearchedUsers.clear();
    mToken.clear();
    mEnabled = false;
    mActiveChannel.clear();
    clearChannels();
}

bool ServerEngine::isConfigured() const
{
    return mEnabled;
}

bool ServerEngine::isConnected() const
{
    return mSocket.state() == QAbstractSocket::ConnectedState && !mToken.isEmpty();
}

ServerEngine::State ServerEngine::state() const
{
    if (isConnected())
        return State::Connected;
    return mSocket.state() == QAbstractSocket::ConnectingState
        || mSocket.state() == QAbstractSocket::HostLookupState
        ? State::Connecting : State::Disconnected;
}

QString ServerEngine::serverUrl() const
{
    return mBaseUrl;
}

QString ServerEngine::token() const
{
    return mToken;
}

long ServerEngine::userId() const
{
    return mUserId;
}

QString ServerEngine::username() const
{
    return mUsername;
}

QString ServerEngine::displayName() const
{
    return mDisplayName;
}

void ServerEngine::login()
{
    mToken.clear();
    mSearchedUsers.clear();
    QUrl url(mBaseUrl + "/api/auth/login");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body.insert("username", mUsername);
    body.insert("password", mPassword);

    auto reply = mHttp.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (mExpectStop)
            return;

        const auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code != 200)
        {
            emit stateChanged(State::Disconnected,
                QString("登录失败（%1）：%2")
                    .arg(code)
                    .arg(QString::fromUtf8(reply->readAll()).left(120)));
            scheduleReconnect();
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        mToken = jsonString(root, "token");
        const QJsonObject user = root.value("user").toObject();
        mUserId = jsonLong(user, "id");
        mUsername = jsonString(user, "username");
        mDisplayName = jsonString(user, "displayName");
        if (mDisplayName.isEmpty())
            mDisplayName = mUsername;

        emit stateChanged(State::Connecting, "拉取频道...");
        fetchChannels();
    });
}

void ServerEngine::fetchChannels()
{
    if (mToken.isEmpty())
        return;

    QNetworkRequest request(QUrl(mBaseUrl + "/api/channels"));
    request.setRawHeader("Authorization", "Bearer " + mToken.toUtf8());

    auto reply = mHttp.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (mExpectStop || mToken.isEmpty())
            return;

        const auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code != 200)
        {
            emit stateChanged(State::Disconnected, QString("获取频道失败（%1）").arg(code));
            scheduleReconnect();
            return;
        }

        const QJsonArray list = QJsonDocument::fromJson(reply->readAll()).array();
        QSet<QString> seen;
        for (int i = 0; i < list.size(); ++i)
        {
            const QJsonObject item = list.at(i).toObject();
            const QString serverId = jsonString(item, "id");
            if (serverId.isEmpty())
                continue;
            seen.insert(serverId);

            auto &channel = mChannels[serverId];
            channel.serverId = serverId;
            channel.kind = jsonString(item, "kind").toLower();
            channel.name = jsonString(item, "name");
            channel.unread = static_cast<int>(jsonLong(item, "unreadCount"));

            const QJsonObject last = item.value("lastMessage").toObject();
            if (!last.isEmpty())
                channel.maxLoadedId = qMax<qint64>(channel.maxLoadedId, jsonLong(last, "id"));

            if (!channel.fellow)
                channel.fellow = make_shared<Fellow>();
            channel.fellow->setIp(remoteKeyOf(serverId).toStdString());
            channel.fellow->setName(channel.name.toStdString());
            channel.fellow->setHost(mBaseUrl.toStdString());
            channel.fellow->setServerId(serverId.toStdString());
            channel.fellow->setServerHost(mBaseUrl.toStdString());
            channel.fellow->setUnreadCount(channel.unread);
            channel.fellow->setOnLine(true);

            if (channel.kind == QLatin1String("direct"))
                fetchChannelDetail(serverId);
            updateChannel(serverId);
        }

        for (auto it = mChannels.begin(); it != mChannels.end();)
        {
            if (seen.contains(it.key()))
            {
                ++it;
                continue;
            }
            const shared_ptr<Fellow> removed = it->fellow;
            it = mChannels.erase(it);
            if (removed)
                emit channelRemoved(removed.get());
        }

        emit stateChanged(State::Connecting, "连接实时通道...");
        connectHub();
    });
}

void ServerEngine::fetchChannelDetail(const QString &channelId)
{
    QNetworkRequest request(QUrl(mBaseUrl + "/api/channels/" + channelId));
    request.setRawHeader("Authorization", "Bearer " + mToken.toUtf8());

    auto reply = mHttp.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelId]() {
        reply->deleteLater();
        if (mExpectStop || mToken.isEmpty())
            return;
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
            return;

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray members = root.value("members").toArray();
        QString otherName;
        QString otherUsername;
        long otherId = 0;
        for (const QJsonValue &value : members)
        {
            const QJsonObject user = value.toObject();
            if (jsonLong(user, "id") == mUserId)
                continue;
            otherUsername = jsonString(user, "username");
            otherName = jsonString(user, "displayName");
            if (otherName.isEmpty())
                otherName = otherUsername;
            otherId = jsonLong(user, "id");
            break;
        }

        auto it = mChannels.find(channelId);
        if (it == mChannels.end())
            return;
        it->otherUserId = otherId;
        it->otherDisplayName = otherName;
        if (it->kind == QLatin1String("direct") && !otherName.isEmpty())
        {
            it->fellow->setServerUserId(otherId);
            it->fellow->setServerUsername(otherUsername.toStdString());
            it->fellow->setName(otherName.toStdString());
            updateChannel(channelId);
        }
    });
}

void ServerEngine::fetchHistory(const QString &channelId, qint64 before, int limit)
{
    QUrl url(mBaseUrl + "/api/channels/" + channelId + "/messages");
    QUrlQuery query;
    if (before > 0)
        query.addQueryItem("before", QString::number(before));
    query.addQueryItem("limit", QString::number(limit));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + mToken.toUtf8());

    auto reply = mHttp.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelId]() {
        reply->deleteLater();
        if (mExpectStop || mToken.isEmpty())
            return;

        const auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code != 200)
        {
            emit loadMoreDone(false, QString("获取历史消息失败（%1）").arg(code));
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray list = root.value("messages").toArray();
        auto it = mChannels.find(channelId);
        if (it == mChannels.end())
            return;

        qint64 oldest = 0;
        for (int i = list.size() - 1; i >= 0; --i)
        {
            const QJsonObject message = list.at(i).toObject();
            const qint64 id = jsonLong(message, "id");
            if (id > 0)
            {
                if (oldest == 0 || id < oldest)
                    oldest = id;
                it->maxLoadedId = qMax(it->maxLoadedId, id);
            }
            const bool mine = jsonLong(message, "senderId") == mUserId;
            auto content = make_shared<TextContent>();
            content->text = jsonString(message, "content").toStdString();
            auto event = make_shared<MessageViewEvent>();
            event->fellow = mine ? nullptr : it->fellow;
            event->contents.push_back(content);
            event->when = messageTime(message);
            dispatch(event);
        }

        if (oldest > 0)
        {
            it->minLoadedId = it->minLoadedId == 0
                ? oldest
                : qMin(it->minLoadedId, oldest);
        }
        const qint64 nextBefore = jsonLong(root, "nextBefore");
        it->hasMoreHistory = nextBefore > 0 && !list.isEmpty();
        it->historyLoaded = true;

        if (channelId == mActiveChannel && mSocket.state() == QAbstractSocket::ConnectedState)
            invoke("JoinChannel", QJsonArray{ channelId });

        emit loadMoreDone(true, it->hasMoreHistory ? QString() : "没有更多历史消息了");
    });
}

void ServerEngine::connectHub()
{
    if (mToken.isEmpty())
        return;

    const QUrl parsed(mBaseUrl);
    QUrl negotiate = parsed;
    negotiate.setPath("/hubs/chat/negotiate");
    negotiate.setQuery("negotiateVersion=1");

    QNetworkRequest request(negotiate);
    request.setRawHeader("Authorization", "Bearer " + mToken.toUtf8());

    auto reply = mHttp.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, parsed]() {
        reply->deleteLater();
        if (mExpectStop || mToken.isEmpty())
            return;

        const auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code != 200)
        {
            emit stateChanged(State::Disconnected, QString("实时通道协商失败（%1）").arg(code));
            scheduleReconnect();
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString token = jsonString(root, "connectionToken");
        if (token.isEmpty())
        {
            emit stateChanged(State::Disconnected, "实时通道协商失败：缺少 connectionToken");
            scheduleReconnect();
            return;
        }

        QUrl wsUrl;
        wsUrl.setScheme(parsed.scheme() == QLatin1String("https") ? "wss" : "ws");
        wsUrl.setHost(parsed.host());
        wsUrl.setPort(parsed.port());
        wsUrl.setPath("/hubs/chat");
        QUrlQuery query;
        query.addQueryItem("id", token);
        query.addQueryItem("access_token", mToken);
        wsUrl.setQuery(query);

        emit stateChanged(State::Connecting, "连接实时通道...");
        mSocket.open(wsUrl);
    });
}

void ServerEngine::disconnectHub()
{
    if (mSocket.state() != QAbstractSocket::UnconnectedState)
    {
        mSocket.abort();
    }
}

void ServerEngine::onWebSocketConnected()
{
    mReconnectAttempt = 0;
    const QByteArray handshake = "{\"protocol\":\"json\",\"version\":1}\x1e";
    sendRecord(handshake);
    emit stateChanged(State::Connected, QStringLiteral("已连接服务器"));
}

void ServerEngine::onWebSocketDisconnected()
{
    if (mExpectStop)
        return;
    emit stateChanged(State::Disconnected, "实时通道已断开");
    if (mEnabled && !mToken.isEmpty())
        scheduleReconnect();
}

void ServerEngine::onWebSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
}

void ServerEngine::tryReconnect()
{
    if (mExpectStop || !mEnabled || mToken.isEmpty())
        return;

    ++mReconnectAttempt;
    emit stateChanged(State::Connecting, "尝试重连...");
    login();
}

void ServerEngine::scheduleReconnect()
{
    if (mExpectStop || !mEnabled || mToken.isEmpty())
        return;
    const int delayMs = qMin(30000, 2000 * (1 << qMin(mReconnectAttempt, 4)));
    mReconnectTimer.start(delayMs);
}

void ServerEngine::sendRecord(const QByteArray &payload)
{
    if (mSocket.state() == QAbstractSocket::ConnectedState)
        mSocket.sendTextMessage(payload);
}

void ServerEngine::invoke(const QString &target, const QJsonArray &args)
{
    QJsonObject frame;
    frame.insert("type", 1);
    frame.insert("invocationId", QString::number(++mInvocationSeq));
    frame.insert("target", target);
    frame.insert("arguments", args);
    sendRecord(QJsonDocument(frame).toJson(QJsonDocument::Compact) + "\x1e");
}

void ServerEngine::onTextMessageReceived(const QString &message)
{
    const QStringList records = message.split(QChar(0x1e), Qt::SkipEmptyParts);
    for (const QString &record : records)
    {
        const QString trimmed = record.trimmed();
        if (trimmed.isEmpty())
            continue;
        const QJsonObject frame = QJsonDocument::fromJson(trimmed.toUtf8()).object();
        if (!frame.isEmpty())
            handleFrame(frame);
    }
}

void ServerEngine::handleFrame(const QJsonObject &frame)
{
    const int type = frame.value("type").toInt();
    if (type == 6)
    {
        // ping
        QJsonObject pong;
        pong.insert("type", 6);
        sendRecord(QJsonDocument(pong).toJson(QJsonDocument::Compact) + "\x1e");
        return;
    }
    if (type != 1)
        return;

    const QString target = jsonString(frame, "target");
    const QJsonArray args = frame.value("arguments").toArray();
    if (target == QLatin1String("ReceiveMessage"))
        handleReceiveMessage(args);
    else if (target == QLatin1String("MessageSent"))
        handleMessageSent(args);
    else if (target == QLatin1String("UserTyping"))
        handleTyping(args);
    else if (target == QLatin1String("UserPresenceChanged"))
        handlePresence(args);
}

void ServerEngine::handleReceiveMessage(const QJsonArray &args)
{
    const QJsonObject message = objectAt(args, 0);
    if (message.isEmpty())
        return;

    const QString channelId = jsonString(message, "channelId");
    const long senderId = jsonLong(message, "senderId");
    const QString clientId = jsonString(message, "clientId");

    if (senderId == mUserId)
    {
        if (!clientId.isEmpty())
            mPendingClientIds.remove(clientId);
        return;
    }

    auto it = mChannels.find(channelId);
    if (it == mChannels.end())
        return;

    const qint64 id = jsonLong(message, "id");
    if (id > 0 && id <= it->maxLoadedId)
        return;
    if (id > 0)
        it->maxLoadedId = qMax(it->maxLoadedId, id);

    if (channelId != mActiveChannel)
    {
        ++it->unread;
        it->fellow->setUnreadCount(it->unread);
        updateChannel(channelId);
    }

    auto content = make_shared<TextContent>();
    content->text = jsonString(message, "content").toStdString();
    auto event = make_shared<MessageViewEvent>();
    event->fellow = it->fellow;
    event->contents.push_back(content);
    event->when = messageTime(message);
    dispatch(event);
}

void ServerEngine::handleMessageSent(const QJsonArray &args)
{
    const QJsonObject result = objectAt(args, 0);
    const QJsonObject message = result.value("message").toObject();
    if (message.isEmpty())
        return;

    const QString channelId = jsonString(message, "channelId");
    const QString clientId = jsonString(message, "clientId");
    if (!clientId.isEmpty())
        mPendingClientIds.remove(clientId);
    const qint64 id = jsonLong(message, "id");

    auto it = mChannels.find(channelId);
    if (it == mChannels.end())
        return;
    if (id > 0)
        it->maxLoadedId = qMax(it->maxLoadedId, id);

    if (it->unread != 0)
    {
        it->unread = 0;
        it->fellow->setUnreadCount(0);
        updateChannel(channelId);
    }
}

void ServerEngine::handleTyping(const QJsonArray &args)
{
    const QJsonObject typing = objectAt(args, 0);
    const QString channelId = jsonString(typing, "channelId");
    const long userId = jsonLong(typing, "userId");
    if (userId == mUserId || channelId.isEmpty())
        return;

    auto it = mChannels.find(channelId);
    if (it == mChannels.end())
        return;

    const QString name = jsonString(typing, "username");
    it->typing = name;
    it->fellow->setTyping(name.toStdString());
    mTypingChannel = channelId;
    mTypingUser = name;
    updateChannel(channelId);
    mTypingClearTimer.start(4000);
}

void ServerEngine::handlePresence(const QJsonArray &args)
{
    const QJsonObject presence = objectAt(args, 0);
    const long userId = jsonLong(presence, "userId");
    const bool online = presence.value("isOnline").toBool();

    for (auto &channel : mChannels)
    {
        if (channel.otherUserId != userId)
            continue;
        channel.onlineMembers.insert(userId, online);
        channel.fellow->setOnLine(online);
        updateChannel(channel.serverId);
    }
}

void ServerEngine::dispatch(shared_ptr<ViewEvent> event)
{
    if (mView)
        mView->onEvent(event);
}

void ServerEngine::updateChannel(const QString &channelId)
{
    auto it = mChannels.find(channelId);
    if (it == mChannels.end() || !it->fellow)
        return;

    auto event = make_shared<FellowViewEvent>();
    event->fellow = it->fellow;
    dispatch(event);
}

void ServerEngine::updateAllChannels()
{
    for (auto it = mChannels.begin(); it != mChannels.end(); ++it)
        updateChannel(it.key());
}

void ServerEngine::clearChannels()
{
    emit channelsCleared();
    mChannels.clear();
}

void ServerEngine::removeChannel(const QString &channelId)
{
    auto it = mChannels.find(channelId);
    if (it == mChannels.end())
        return;
    const shared_ptr<Fellow> removed = it->fellow;
    mChannels.erase(it);
    if (removed)
        emit channelRemoved(removed.get());
}

void ServerEngine::reloadChannels()
{
    if (mToken.isEmpty())
    {
        emit stateChanged(State::Disconnected, "尚未登录服务器");
        return;
    }
    fetchChannels();
}

void ServerEngine::searchUsers(const QString &query)
{
    mSearchedUsers.clear();
    QUrl url(mBaseUrl + "/api/users");
    QUrlQuery params;
    params.addQueryItem("query", query);
    url.setQuery(params);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + mToken.toUtf8());

    auto reply = mHttp.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QList<const Fellow*> result;
        if (mExpectStop || mToken.isEmpty())
        {
            emit usersSearched(result);
            return;
        }

        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
        {
            const QJsonArray list = QJsonDocument::fromJson(reply->readAll()).array();
            for (const QJsonValue &value : list)
            {
                const QJsonObject user = value.toObject();
                auto fellow = make_shared<Fellow>();
                QString display = jsonString(user, "displayName");
                if (display.isEmpty())
                    display = jsonString(user, "username");
                const long id = jsonLong(user, "id");
                fellow->setName(display.toStdString());
                fellow->setHost(mBaseUrl.toStdString());
                fellow->setOnLine(user.value("isOnline").toBool());
                fellow->setServerHost(mBaseUrl.toStdString());
                fellow->setServerId(QString("user:%1").arg(id).toStdString());
                fellow->setServerUsername(jsonString(user, "username").toStdString());
                fellow->setServerUserId(id);
                mSearchedUsers.append(fellow);
                result.append(fellow.get());
            }
        }
        emit usersSearched(result);
    });
}

void ServerEngine::openDirectChannel(long remoteUserId, const QString &displayName,
                                     const QString &username)
{
    if (mToken.isEmpty())
    {
        emit directChannelOpened(nullptr, false, "未连接服务器");
        return;
    }

    QNetworkRequest request(QUrl(mBaseUrl + "/api/channels"));
    request.setRawHeader("Authorization", "Bearer " + mToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body.insert("kind", "Direct");
    body.insert("memberIds", QJsonArray{ QJsonValue(static_cast<qint64>(remoteUserId)) });
    auto reply = mHttp.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, remoteUserId, displayName, username]() {
        reply->deleteLater();
        if (mExpectStop || mToken.isEmpty())
        {
            emit directChannelOpened(nullptr, false, "连接已断开");
            return;
        }

        const auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code != 200 && code != 201)
        {
            emit directChannelOpened(nullptr, false, QString("创建会话失败（%1）").arg(code));
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString serverId = jsonString(root, "id");
        if (serverId.isEmpty())
        {
            emit directChannelOpened(nullptr, false, "创建会话失败：响应缺少 id");
            return;
        }

        auto &channel = mChannels[serverId];
        channel.serverId = serverId;
        channel.kind = "direct";
        channel.name = displayName;
        channel.unread = 0;
        if (!channel.fellow)
            channel.fellow = make_shared<Fellow>();
        channel.fellow->setIp(remoteKeyOf(serverId).toStdString());
        channel.fellow->setName(displayName.toStdString());
        channel.fellow->setHost(mBaseUrl.toStdString());
        channel.fellow->setServerId(serverId.toStdString());
        channel.fellow->setServerHost(mBaseUrl.toStdString());
        channel.fellow->setServerUsername(username.toStdString());
        channel.fellow->setServerUserId(remoteUserId);
        channel.fellow->setOnLine(true);
        channel.fellow->setUnreadCount(0);

        if (mSocket.state() == QAbstractSocket::ConnectedState)
            invoke("JoinChannel", QJsonArray{ serverId });
        updateChannel(serverId);
        emit directChannelOpened(channel.fellow.get(), true, QString());
    });
}

void ServerEngine::addDirectChannel(const QString &username, const QString &displayName)
{
    searchUsers(username);
}

void ServerEngine::sendTyping(const Fellow *fellow)
{
    if (!fellow || !fellow->isRemote() || mSocket.state() != QAbstractSocket::ConnectedState)
        return;
    const QString channelId = channelIdOf(QString::fromStdString(fellow->getIp()));
    if (channelId.isEmpty())
        return;
    invoke("Typing", QJsonArray{ channelId });
}

pair<bool, QString> ServerEngine::sendText(const Fellow *fellow, const QString &text)
{
    if (!fellow || !fellow->isRemote())
        return { false, "当前会话不是服务器会话" };

    const QString channelId = channelIdOf(QString::fromStdString(fellow->getIp()));
    if (channelId.isEmpty())
        return { false, "会话信息无效" };

    const bool joined = mChannels.contains(channelId);
    if (!joined || mSocket.state() != QAbstractSocket::ConnectedState)
        return { false, "服务器连接不可用，请重新连接" };

    const QString clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    mPendingClientIds.insert(clientId);
    invoke("SendMessage", QJsonArray{ channelId, text, clientId });
    return { true, QString() };
}

pair<bool, QString> ServerEngine::sendKnock(const Fellow *fellow)
{
    const auto fallback = sendText(fellow, "（窗口抖动）");
    return { fallback.first, fallback.second };
}

void ServerEngine::openChannel(const Fellow *fellow)
{
    if (!fellow || !fellow->isRemote())
        return;
    const QString channelId = channelIdOf(QString::fromStdString(fellow->getIp()));
    if (channelId.isEmpty())
        return;

    auto it = mChannels.find(channelId);
    if (it == mChannels.end())
        return;
    mActiveChannel = channelId;

    if (it->otherUserId == 0 && it->kind == QLatin1String("direct"))
        fetchChannelDetail(channelId);

    if (!it->historyLoaded)
    {
        fetchHistory(channelId, 0, 100);
    }

    if (it->unread != 0)
    {
        it->unread = 0;
        it->fellow->setUnreadCount(0);
        markRead(fellow);
        updateChannel(channelId);
    }

    if (mSocket.state() == QAbstractSocket::ConnectedState)
        invoke("JoinChannel", QJsonArray{ channelId });
}

void ServerEngine::loadMoreHistory(const Fellow *fellow)
{
    if (!fellow || !fellow->isRemote())
        return;
    const QString channelId = channelIdOf(QString::fromStdString(fellow->getIp()));
    auto it = mChannels.find(channelId);
    if (it == mChannels.end())
        return;
    fetchHistory(channelId, it->minLoadedId > 0 ? it->minLoadedId : 0, 100);
}

void ServerEngine::markRead(const Fellow *fellow)
{
    if (!fellow || !fellow->isRemote())
        return;
    const QString channelId = channelIdOf(QString::fromStdString(fellow->getIp()));
    if (channelId.isEmpty())
        return;

    QNetworkRequest request(QUrl(mBaseUrl + "/api/channels/" + channelId + "/read"));
    request.setRawHeader("Authorization", "Bearer " + mToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto reply = mHttp.post(request, QByteArray("{}"));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

shared_ptr<Fellow> ServerEngine::getShared(const Fellow *fellow) const
{
    if (!fellow)
        return nullptr;
    for (const auto &pair : mChannels)
    {
        if (pair.fellow.get() == fellow || pair.fellow->getIp() == fellow->getIp())
            return pair.fellow;
    }
    return nullptr;
}

const Fellow *ServerEngine::findChannelFellow(const QString &serverId) const
{
    auto it = mChannels.find(serverId);
    return it == mChannels.end() ? nullptr : it->fellow.get();
}

QList<const Fellow*> ServerEngine::searchFellowByName(const QString &name) const
{
    QList<const Fellow*> result;
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
        return result;
    for (const auto &fellow : mSearchedUsers)
    {
        const QString username = QString::fromStdString(fellow->getServerUsername());
        const QString display = QString::fromStdString(fellow->getName());
        if (username.compare(wanted, Qt::CaseInsensitive) == 0
            || display.compare(wanted, Qt::CaseInsensitive) == 0)
        {
            result.append(fellow.get());
        }
    }
    return result;
}

QList<const Fellow*> ServerEngine::channelFellows() const
{
    QList<const Fellow*> result;
    for (const auto &pair : mChannels)
        result.append(pair.fellow.get());
    return result;
}

vector<const Fellow*> ServerEngine::searchChannels(const QString &text) const
{
    vector<const Fellow*> result;
    for (const auto &pair : mChannels)
    {
        const QString name = QString::fromStdString(pair.fellow->getName());
        if (text.isEmpty() || name.contains(text, Qt::CaseInsensitive))
            result.push_back(pair.fellow.get());
    }
    return result;
}

std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>
ServerEngine::messageTime(const QJsonObject &message)
{
    using Clock = std::chrono::system_clock;
    using Millis = std::chrono::milliseconds;

    QDateTime parsed = QDateTime::fromString(jsonString(message, "sentAtUtc"), Qt::ISODateWithMs);
    if (!parsed.isValid())
        parsed = QDateTime::fromString(jsonString(message, "sentAtUtc"), Qt::ISODate);
    if (!parsed.isValid())
        return time_point_cast<Millis>(Clock::now());
    return time_point<Clock, Millis>(Millis(parsed.toUTC().toMSecsSinceEpoch()));
}

QString ServerEngine::remoteKeyOf(const QString &channelId)
{
    return QString("server://") + channelId;
}

QString ServerEngine::channelIdOf(const QString &remoteKey)
{
    const QString prefix = "server://";
    return remoteKey.startsWith(prefix) ? remoteKey.mid(prefix.size()) : QString();
}

