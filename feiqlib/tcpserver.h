#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

class QTcpServer;
class TcpSocket;

class TcpServer
{
public:
    TcpServer();
    ~TcpServer();
    typedef std::function<void (std::unique_ptr<TcpSocket>)> ClientHandler;
public:
    bool start(int port);
    void whenNewClient(ClientHandler onClientConnected);
    void stop();
private:
    void keepAccept();
private:
    ClientHandler mClientHandler;
    std::atomic<bool> mStarted{false};
    QTcpServer* mServer=nullptr;
    std::thread mAcceptThread;
    std::mutex mServerMutex;
};

#endif // TCPSERVER_H
