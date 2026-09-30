#include "tcpsocket.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QString>

TcpSocket::TcpSocket()
{

}

TcpSocket::TcpSocket(QTcpSocket *socket)
{
    mSocket = socket;
    if (mSocket != nullptr)
    {
        mPeerIp = mSocket->peerAddress().toString().toStdString();
    }
}

TcpSocket::~TcpSocket()
{
    disconnect();
}

bool TcpSocket::connect(const string &ip, int port)
{
    if (mSocket != nullptr)
        return true;

    auto socket = new QTcpSocket();
    socket->connectToHost(QString::fromStdString(ip), static_cast<quint16>(port));
    if (!socket->waitForConnected(3000))
    {
        delete socket;
        return false;
    }

    mSocket = socket;
    mPeerIp = ip;
    return true;
}

void TcpSocket::disconnect()
{
    if (mSocket != nullptr)
    {
        mSocket->abort();
        delete mSocket;
        mSocket = nullptr;
        mPeerIp.clear();
    }
}

int TcpSocket::send(const void *data, int size)
{
    if (mSocket == nullptr)
        return -1;

    int sent = 0;
    auto pdata = static_cast<const char*>(data);

    while (sent < size)
    {
        auto ret = mSocket->write(pdata+sent, size-sent);
        if (ret <= 0)
        {
            return -1;
        }

        sent += static_cast<int>(ret);
        if (!mSocket->waitForBytesWritten(5000))
            return -1;
    }

    return sent;
}

int TcpSocket::recv(void *data, int size, int msTimeout)
{
    if (mSocket == nullptr)
        return -2;

    if (!mSocket->waitForReadyRead(msTimeout))
    {
        if (mSocket->error() == QAbstractSocket::SocketTimeoutError)
            return -1;
        return -2;
    }

    auto ret = mSocket->read(static_cast<char*>(data), size);
    if (ret <= 0)
    {
        return -2;
    }

    return static_cast<int>(ret);
}
