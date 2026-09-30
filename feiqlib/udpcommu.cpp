#include "udpcommu.h"
#include <QUdpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QString>
#include <array>
#include <mutex>

#define setFailedMsgAndReturnFalse(msg) \
    {mErrMsg = msg;\
    return false;}

UdpCommu::UdpCommu(){}

UdpCommu::~UdpCommu()
{
    close();
}

bool UdpCommu::bindTo(int port)
{
    if (mSocket.load(std::memory_order_relaxed) != nullptr)
        setFailedMsgAndReturnFalse("已经初始化");

    auto socket = new QUdpSocket();
    socket->setSocketOption(QAbstractSocket::BroadcastSocketOption, 1);
    auto ret = socket->bind(QHostAddress::AnyIPv4, static_cast<quint16>(port),
                            QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);
    if (!ret)
    {
        mErrMsg = socket->errorString().toStdString();
        delete socket;
        return false;
    }

    mSocket.store(socket, std::memory_order_release);
    return true;
}

int UdpCommu::sentTo(const string& ip, int port, const void *data, int size)
{
    auto socket = mSocket.load(std::memory_order_acquire);
    if (socket == nullptr)
    {
        mErrMsg = "请先初始化socket";
        return -1;
    }

    lock_guard<mutex> lock(mSendMutex);
    auto ret = socket->writeDatagram(static_cast<const char*>(data), size,
                                     QHostAddress(QString::fromStdString(ip)),
                                     static_cast<quint16>(port));
    if (ret < 0)
        mErrMsg = socket->errorString().toStdString();

    return static_cast<int>(ret);
}

bool UdpCommu::startAsyncRecv(UdpRecvHandler handler)
{
    if (handler == nullptr)
        setFailedMsgAndReturnFalse("handler不能为空")

    if (mSocket.load(std::memory_order_acquire) == nullptr)
        setFailedMsgAndReturnFalse("请先初始化socket");

    {
        lock_guard<mutex> lock(mHandlerMutex);
        mRecvHandler = handler;
    }
    if (!mAsyncMode)
    {
        mAsyncMode = true;
        mRecvThread = std::thread(&UdpCommu::recvThread, this);
    }

    return true;
}

void UdpCommu::close()
{
    if (mSocket.load(std::memory_order_relaxed) == nullptr && !mRecvThread.joinable())
        return;

    mAsyncMode = false;
    if (mRecvThread.joinable())
        mRecvThread.join();

    auto socket = mSocket.exchange(nullptr, std::memory_order_acq_rel);
    if (socket != nullptr)
    {
        delete socket;
    }
}

string UdpCommu::getBoundMac()
{
    auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces)
    {
        auto flags = iface.flags();
        if (flags & QNetworkInterface::IsLoopBack)
            continue;
        if (!(flags & (QNetworkInterface::IsUp | QNetworkInterface::IsRunning)))
            continue;

        auto mac = iface.hardwareAddress().toLower().toStdString();
        if (mac.empty())
            continue;

        string compact;
        for (auto ch : mac)
        {
            if (ch != ':' && ch != '-' && ch != '.')
                compact.push_back(ch);
        }
        if (!compact.empty())
            return compact;
    }

    return "";
}

string UdpCommu::getErrMsg()
{
    return mErrMsg;
}

void UdpCommu::recvThread()
{
    std::array<char,MAX_RCV_SIZE> buf;
    while (mAsyncMode) {
        auto socket = mSocket.load(std::memory_order_acquire);
        if (socket == nullptr)
            break;

        buf.fill(0);
        if (!socket->waitForReadyRead(500))
        {
            if (!mAsyncMode)
                break;
            if (socket->error() != QAbstractSocket::NoError)
                socket->clearError();
            continue;
        }

        while (socket->hasPendingDatagrams()) {
            auto pendingSize = socket->pendingDatagramSize();
            if (pendingSize <= 0 || pendingSize > MAX_RCV_SIZE)
            {
                socket->readDatagram(buf.data(), MAX_RCV_SIZE);
                continue;
            }

            QHostAddress addr;
            quint16 port = 0;
            auto size = socket->readDatagram(buf.data(), pendingSize, &addr, &port);
            if (size < 0)
            {
                if (mAsyncMode)
                    socket->clearError();
                break;
            }

            auto ip = addr.toString().toStdString();
            vector<char> data(std::begin(buf), std::begin(buf)+size);
            UdpRecvHandler handler;
            {
                lock_guard<mutex> lock(mHandlerMutex);
                handler = mRecvHandler;
            }
            if (handler)
                handler(ip, data);
        }
    }

    mAsyncMode=false;
}
