#include "platformdepend.h"
#include "mainwindow.h"
#include <QSystemTrayIcon>
#include <QIcon>

PlatformDepend::PlatformDepend()
{
    mTray = nullptr;
    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
        mTray = new QSystemTrayIcon(QIcon(":/default/res/icon.png"));
        mTray->setToolTip(QStringLiteral("我Q"));
        mTray->show();
    }
}

PlatformDepend::~PlatformDepend()
{
    delete mTray;
}

PlatformDepend &PlatformDepend::instance()
{
    static PlatformDepend me;
    return me;
}

long PlatformDepend::showNotify(const QString &title, const QString &content, const QString &fellowIp)
{
    if (mTray == nullptr)
        return 0;

    mLastFellowIp = fellowIp;
    mTray->showMessage(title, content, QSystemTrayIcon::Information, 8000);
    return ++mNextNotifyId;
}

void PlatformDepend::hideAllNotify()
{
    // QSystemTrayIcon 没有通用的隐藏消息气泡接口，这里保留占位
}

void PlatformDepend::setBadgeNumber(int number)
{
    Q_UNUSED(number);
    // Windows/Linux 桌面没有原生 dock badge，暂不处理
}

void PlatformDepend::setMainWnd(MainWindow *mainWnd)
{
    mMainWnd = mainWnd;
    if (mTray == nullptr)
        return;

    auto openMainWindow = [this](){
        if (mMainWnd != nullptr && !mLastFellowIp.isEmpty())
            mMainWnd->onNotifyClicked(mLastFellowIp);
    };
    QObject::connect(mTray, &QSystemTrayIcon::messageClicked, openMainWindow);
    QObject::connect(mTray, &QSystemTrayIcon::activated, [this, openMainWindow](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            openMainWindow();
    });
}
