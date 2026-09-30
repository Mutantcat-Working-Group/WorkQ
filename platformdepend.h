#ifndef PLATFORMDEPEND_H
#define PLATFORMDEPEND_H

#include <QString>
class MainWindow;
class QSystemTrayIcon;

class IPlatform
{
public:
    virtual ~IPlatform(){}

    virtual long showNotify(const QString& title, const QString& content, const QString & fellowIp) = 0;
    virtual void hideAllNotify() = 0;

    virtual void setBadgeNumber(int number) = 0;

    virtual void setMainWnd(MainWindow* mainWnd)
    {
        Q_UNUSED(mainWnd);
    }
};

class PlatformDepend : public IPlatform
{
public:
    long showNotify(const QString& title, const QString& content, const QString & fellowIp) override;
    void hideAllNotify() override;

    void setBadgeNumber(int number) override;

    void setMainWnd(MainWindow* mainWnd) override;

    static PlatformDepend& instance();
private:
    PlatformDepend();
    ~PlatformDepend();

    QSystemTrayIcon* mTray;
    MainWindow* mMainWnd = nullptr;
    long mNextNotifyId = 0;
    QString mLastFellowIp;
};

#endif // PLATFORMDEPEND_H
