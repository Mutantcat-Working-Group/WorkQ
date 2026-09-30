#ifndef SERVERENGINE_H
#define SERVERENGINE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QList>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>
#include "feiqlib/ifeiqview.h"

class ServerSettingsDialog;

/**
 * WorkQ-Server 模式的引擎。对外复用 FeiqEngine 的视图事件约定：
 * 频道被映射为 Fellow，远程消息被映射为 TextContent，
 * MainWindow 不需要感知服务器协议细节。
 */
class ServerEngine : public QObject
{
    Q_OBJECT

public:
    enum class State { Disconnected, Connecting, Connected };
    Q_ENUM(State)

    explicit ServerEngine(QObject *parent = nullptr);
    ~ServerEngine() override;

    void start(const QString &url, const QString &username, const QString &password);
    void stop();
    bool isConfigured() const;
    bool isConnected() const;
    State state() const;

    QString serverUrl() const;
    QString token() const;
    long userId() const;
    QString username() const;
    QString displayName() const;

    void setView(IFeiqView *view);

    pair<bool, QString> sendText(const Fellow *fellow, const QString &text);
    pair<bool, QString> sendKnock(const Fellow *fellow);
    void openChannel(const Fellow *fellow);
    void loadMoreHistory(const Fellow *fellow);
    void markRead(const Fellow *fellow);
    void reloadChannels();
    void searchUsers(const QString &query);
    void openDirectChannel(long remoteUserId, const QString &displayName,
                           const QString &username);
    void addDirectChannel(const QString &username, const QString &displayName);
    void sendTyping(const Fellow *fellow);

    shared_ptr<Fellow> getShared(const Fellow *fellow) const;
    const Fellow *findChannelFellow(const QString &serverId) const;
    QList<const Fellow*> channelFellows() const;
    QList<const Fellow*> searchFellowByName(const QString &name) const;
    vector<const Fellow*> searchChannels(const QString &text) const;

signals:
    void stateChanged(ServerEngine::State state, const QString &detail);
    void usersSearched(QList<const Fellow*> users);
    void directChannelOpened(const Fellow *fellow, bool ok, const QString &error);
    void loadMoreDone(bool ok, const QString &error);
    void channelsCleared();
    void channelRemoved(const Fellow *fellow);

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onTextMessageReceived(const QString &message);
    void tryReconnect();

private:
    struct ChannelData
    {
        shared_ptr<Fellow> fellow;
        QString serverId;
        QString kind;
        QString name;
        long otherUserId = 0;
        QString otherDisplayName;
        int unread = 0;
        qint64 maxLoadedId = 0;
        qint64 minLoadedId = 0;
        bool hasMoreHistory = true;
        bool historyLoaded = false;
        QString typing;
        QHash<long, bool> onlineMembers;
    };

    void login();
    void fetchChannels();
    void fetchChannelDetail(const QString &channelId);
    void fetchHistory(const QString &channelId, qint64 before, int limit);
    void connectHub();
    void disconnectHub();
    void sendRecord(const QByteArray &payload);
    void invoke(const QString &target, const QJsonArray &args);
    void handleFrame(const QJsonObject &frame);
    void handleReceiveMessage(const QJsonObject &args);
    void handleMessageSent(const QJsonObject &args);
    void handleTyping(const QJsonObject &args);
    void handlePresence(const QJsonObject &args);
    void dispatch(shared_ptr<ViewEvent> event);
    void updateChannel(const QString &channelId);
    void updateAllChannels();
    void clearChannels();
    void removeChannel(const QString &channelId);

    static std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>
    messageTime(const QJsonObject &message);

    static QString remoteKeyOf(const QString &channelId);
    static QString channelIdOf(const QString &remoteKey);

    QString mBaseUrl;
    QString mUsername;
    QString mPassword;
    QString mToken;
    QString mActiveChannel;
    long mUserId = 0;
    QString mDisplayName;
    bool mEnabled = false;
    bool mExpectStop = false;
    int mReconnectAttempt = 0;

    IFeiqView *mView = nullptr;
    QNetworkAccessManager mHttp;
    QWebSocket mSocket;
    QTimer mReconnectTimer;
    QTimer mTypingClearTimer;
    QHash<QString, ChannelData> mChannels;
    QSet<QString> mPendingClientIds;
    int mInvocationSeq = 0;
    QString mTypingChannel;
    QString mTypingUser;
    QList<shared_ptr<Fellow>> mSearchedUsers;
};

#endif // SERVERENGINE_H
