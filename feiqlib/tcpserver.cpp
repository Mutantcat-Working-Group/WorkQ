#include "tcpserver.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include "tcpsocket.h"

TcpServer::TcpServer()
{

}

TcpServer::~TcpServer()
{
    stop();
}

bool TcpServer::start(int port)
{
    if (mStarted)
        return true;

    auto server = new QTcpServer();
    if (!server->listen(QHostAddress::AnyIPv4, static_cast<quint16>(port)))
    {
        delete server;
        return false;
    }

    {
        lock_guard<mutex> lock(mServerMutex);
        mServer = server;
    }
    mStarted = true;
    mAcceptThread = std::thread(&TcpServer::keepAccept, this);
    return true;
}

void TcpServer::whenNewClient(TcpServer::ClientHandler onClientConnected)
{
    lock_guard<mutex> lock(mServerMutex);
    mClientHandler = onClientConnected;
}

void TcpServer::stop()
{
    mStarted = false;
    if (mAcceptThread.joinable())
        mAcceptThread.join();

    QTcpServer* server = nullptr;
    {
        lock_guard<mutex> lock(mServerMutex);
        server = mServer;
        mServer = nullptr;
    }
    delete server;
}

void TcpServer::keepAccept()
{
    while (mStarted)
    {
        QTcpServer* server;
        ClientHandler handler;
        {
            lock_guard<mutex> lock(mServerMutex);
            server = mServer;
            handler = mClientHandler;
        }

        if (server == nullptr)
            break;

        if (!server->waitForNewConnection(500))
        {
            if (!mStarted)
                break;
            continue;
        }

        while (server->hasPendingConnections())
        {
            auto socket = server->nextPendingConnection();
            if (socket == nullptr)
                break;
            socket->setParent(nullptr);

            if (handler)
                handler(std::unique_ptr<TcpSocket>(new TcpSocket(socket)));
        }
    }
}
