#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <thread>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QThread>
#include <QMetaObject>
#include <QTimer>
#include "addfellowdialog.h"
#include "platformdepend.h"
#include "feiqwin.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qRegisterMetaType<shared_ptr<ViewEvent>>("ViewEventSharedPtr");

    connect(this, SIGNAL(showErrorAndQuit(QString)), this, SLOT(onShowErrorAndQuit(QString)));

    //加载配置，兼容旧的 .feiq_setting.ini
    auto settingFilePath = QDir::home().filePath(".workq_setting.ini");
    auto oldSettingFilePath = QDir::home().filePath(".feiq_setting.ini");
    if (!QFile::exists(settingFilePath) && QFile::exists(oldSettingFilePath))
    {
        QFile oldFile(oldSettingFilePath);
        if (oldFile.open(QIODevice::ReadOnly))
        {
            QFile newFile(settingFilePath);
            if (newFile.open(QIODevice::WriteOnly))
            {
                newFile.write(oldFile.readAll().replace("mac飞秋", "我Q"));
                newFile.close();
            }
            oldFile.close();
        }
    }
    mSettings = new Settings(settingFilePath, QSettings::IniFormat);
    mTitle = mSettings->value("app/title", "我Q").toString();
    setWindowTitle(mTitle);

    //初始化搜索对话框
    mSearchFellowDlg = new SearchFellowDlg(this);
    connect(mSearchFellowDlg, SIGNAL(onFellowSelected(const Fellow*)),
            this, SLOT(finishSearch(const Fellow*)));

    connect(ui->actionRefreshFellows, SIGNAL(triggered(bool)), this, SLOT(refreshFellowList()));
    connect(ui->actionAddFellow, SIGNAL(triggered(bool)), this, SLOT(addFellow()));

    mSearchFellowDlg->setSearchDriver(std::bind(&MainWindow::fellowSearchDriver, this, placeholders::_1));

    //初始化文件管理对话框
    mDownloadFileDlg = new FileManagerDlg(this);
    mDownloadFileDlg->setEngine(&mFeiq);
    connect(this, SIGNAL(statChanged(FileTask*)), mDownloadFileDlg, SLOT(statChanged(FileTask*)));
    connect(this, SIGNAL(progressChanged(FileTask*)), mDownloadFileDlg, SLOT(progressChanged(FileTask*)));

    //初始化好友列表
    mFellowList.bindTo(ui->fellowListWidget);
    connect(&mFellowList, SIGNAL(select(const Fellow*)), this, SLOT(openChartTo(const Fellow*)));

    //初始化接收文本框
    mRecvTextEdit = ui->recvEdit;
    connect(mRecvTextEdit, SIGNAL(navigateToFileTask(IdType,IdType,bool)), this, SLOT(navigateToFileTask(IdType,IdType,bool)));

    //初始化发送文本框
    mSendTextEdit = ui->sendEdit;
    connect(mSendTextEdit, SIGNAL(acceptDropFiles(QList<QFileInfo>)), this, SLOT(sendFiles(QList<QFileInfo>)));
    if (mSettings->value("app/send_by_enter", true).toBool())
    {
        connect(mSendTextEdit, SIGNAL(enterPressed()), this, SLOT(sendText()));
        connect(mSendTextEdit, SIGNAL(ctrlEnterPressed()), mSendTextEdit, SLOT(newLine()));
    }
    else
    {
        connect(mSendTextEdit, SIGNAL(ctrlEnterPressed()), this, SLOT(sendText()));
        connect(mSendTextEdit, SIGNAL(enterPressed()), mSendTextEdit, SLOT(newLine()));
    }
    connect(mSendTextEdit, &QTextEdit::textChanged, this, [this]() {
        if (mSendTextEdit->toPlainText().isEmpty())
            return;
        const Fellow *fellow = mRecvTextEdit->curFellow();
        if (fellow && fellow->isRemote())
            mServer.sendTyping(fellow);
    });

    //初始化Emoji对话框
    mChooseEmojiDlg = new ChooseEmojiDlg(this);
    connect(ui->actionInsertEmoji, SIGNAL(triggered(bool)), this, SLOT(openChooseEmojiDlg()));
    connect(mChooseEmojiDlg, SIGNAL(choose(QString)),mSendTextEdit, SLOT(insertPlainText(QString)));

    //初始化菜单
    connect(ui->actionSearchFellow, SIGNAL(triggered(bool)), this, SLOT(openSearchDlg()));
    connect(ui->actionSettings, SIGNAL(triggered(bool)), this, SLOT(openSettings()));
    connect(ui->actionOpendl, SIGNAL(triggered(bool)), this, SLOT(openDownloadDlg()));
    connect(ui->actionSendText, SIGNAL(triggered(bool)), this, SLOT(sendText()));
    connect(ui->actionSendKnock, SIGNAL(triggered(bool)), this, SLOT(sendKnock()));
    connect(ui->actionSendFile, SIGNAL(triggered(bool)), this, SLOT(sendFile()));
    connect(ui->actionLoadMoreHistory, SIGNAL(triggered(bool)), this, SLOT(loadMoreHistory()));
    connect(ui->actionConnectServer, SIGNAL(triggered(bool)), this, SLOT(initServer()));
    connect(ui->actionDisconnectServer, SIGNAL(triggered(bool)), this, SLOT(disconnectServer()));
    connect(&mServer, &ServerEngine::stateChanged,
            this, &MainWindow::onServerStateChanged);
    connect(&mServer, &ServerEngine::usersSearched,
            this, [this](QList<const Fellow*> users) {
        mServerSearchResult = users;
        if (mPendingServerAddUser.isEmpty())
            return;

        const QString wanted = mPendingServerAddUser;
        mPendingServerAddUser.clear();
        const Fellow *match = nullptr;
        for (const Fellow *user : users)
        {
            const QString name = QString::fromStdString(user->getName());
            const QString username = QString::fromStdString(user->getServerUsername());
            if (name.compare(wanted, Qt::CaseInsensitive) == 0
                || username.compare(wanted, Qt::CaseInsensitive) == 0)
            {
                match = user;
                break;
            }
        }
        if (!match && !users.isEmpty())
            match = users.first();

        if (match)
        {
            mServer.openDirectChannel(match->getServerUserId(),
                                      QString::fromStdString(match->getName()),
                                      QString::fromStdString(match->getServerUsername()));
        }
        else
        {
            mRecvTextEdit->addWarning(QString("服务器上找不到用户：%1").arg(wanted));
        }
    });
    connect(&mServer, &ServerEngine::directChannelOpened,
            this, [this](const Fellow *fellow, bool ok, const QString &error) {
        if (!ok)
        {
            mRecvTextEdit->addWarning(error);
            return;
        }
        mFellowList.update(*fellow);
        mFellowList.top(*fellow);
        openChartTo(fellow);
    });
    connect(&mServer, &ServerEngine::loadMoreDone,
            this, [this](bool ok, const QString &error) {
        if (!ok)
            mRecvTextEdit->addWarning(error);
    });
    connect(&mServer, &ServerEngine::channelsCleared,
            this, [this]() {
        mFellowList.removeRemote();
        mPendingServerAddUser.clear();
    });
    connect(&mServer, &ServerEngine::channelRemoved,
            this, [this](const Fellow *fellow) {
        if (fellow)
            mFellowList.remove(*fellow);
    });

    //初始化平台相关特性
    PlatformDepend::instance().setMainWnd(this);
    mServerStatusLabel = new QLabel(this);
    statusBar()->addWidget(mServerStatusLabel);
    mServerStatusLabel->setText("服务器未连接");
    mServerStatusLabel->setStyleSheet("color: gray;");

    //初始化我Q引擎
    connect(this, SIGNAL(feiqViewEvent(shared_ptr<ViewEvent>)), this, SLOT(handleFeiqViewEvent(shared_ptr<ViewEvent>)));

    //初始化通信放到 GUI 事件循环里执行，避免窗口销毁时后台线程还在访问 this
    QTimer::singleShot(0, this, &MainWindow::initFeiq);
}

MainWindow::~MainWindow()
{
    if (mFeiqWin)
        mFeiqWin->unInit();
    mServer.stop();
    mFeiq.stop();
    mSettings->sync();
    delete mSettings;
    delete mSearchFellowDlg;
    delete mDownloadFileDlg;
    delete mChooseEmojiDlg;
    delete ui;
}

void MainWindow::setFeiqWin(FeiqWin *feiqWin)
{
    mFeiqWin = feiqWin;
    mFeiqWin->init(this);
}

void MainWindow::onNotifyClicked(const QString& fellowIp)
{
    qDebug()<<fellowIp;
    shared_ptr<Fellow> fellow;
    if (fellowIp.startsWith("server://"))
        fellow = mServer.getShared(mServer.findChannelFellow(fellowIp));
    else
        fellow = mFeiq.getModel().findFirstFellowOf(fellowIp.toStdString());
    if (fellow)
        openChartTo(fellow.get());

    raise();
    activateWindow();
    showNormal();
}

void MainWindow::onNotifyReplied(long notifyId, const QString &fellowIp, const QString &reply)
{
    shared_ptr<Fellow> fellow;
    if (fellowIp.startsWith("server://"))
        fellow = mServer.getShared(mServer.findChannelFellow(fellowIp));
    else
        fellow = mFeiq.getModel().findFirstFellowOf(fellowIp.toStdString());
    if (fellow)
    {
        //回复消息
        auto content = make_shared<TextContent>();
        content->text = reply.toStdString();

        if (fellow->isRemote())
            mServer.sendText(fellow.get(), reply);
        else
            mFeiq.send(fellow, content);

        //设为已回复
        auto msgRepliedTo = findUnshownMessage(notifyId);
        if (msgRepliedTo)
        {
            msgRepliedTo->replied=true;
        }

        //将自己的回复放入未显示列表
        auto event = make_shared<MessageViewEvent>();
        event->contents.push_back(content);
        event->fellow = nullptr;

        auto& msg = addUnshownMessage(fellow.get(), event);
        msg.read = true;

        updateUnshownHint(fellow.get());
    }
}

void MainWindow::enterEvent(QEvent *event)
{
    auto fellow = mRecvTextEdit->curFellow();
    if (fellow)
    {
        flushUnshown(fellow);
        updateUnshownHint(fellow);
    }

    PlatformDepend::instance().hideAllNotify();
}

void MainWindow::openChartTo(const Fellow *fellow)
{
    if (fellow == nullptr)
        return;

    mFellowList.top(*fellow);
    mRecvTextEdit->setCurFellow(fellow);
    setWindowTitle(mTitle + " - 与"+fellow->getName().c_str()+"会话中");
    flushUnshown(fellow);
    updateUnshownHint(fellow);
    if (fellow->isRemote())
        mServer.openChannel(fellow);
}

shared_ptr<Fellow> MainWindow::checkCurFellow()
{
    const Fellow *current = mRecvTextEdit->curFellow();
    auto fellow = current && current->isRemote()
        ? mServer.getShared(current)
        : mFeiq.getModel().getShared(current);
    if (fellow == nullptr)
    {
        mRecvTextEdit->addWarning("这是要发给谁？");
    }

    return fellow;
}

void MainWindow::showResult(pair<bool, string> ret, const Content* content)
{
    if (ret.first)
        mRecvTextEdit->addMyContent(content, QDateTime::currentDateTime().currentMSecsSinceEpoch());
    else
        mRecvTextEdit->addWarning(ret.second.c_str());
}

void MainWindow::onStateChanged(FileTask *fileTask)
{
    if (QThread::currentThread() != this->thread())
    {
        QMetaObject::invokeMethod(this, [this, fileTask](){
            onStateChanged(fileTask);
        }, Qt::QueuedConnection);
        return;
    }

    if (fileTask->getState()==FileTaskState::Finish)
    {
        auto content = fileTask->getContent();
        auto fellow = fileTask->fellow();
        if (content == nullptr || fellow == nullptr)
            return;

        auto title = QString(fileTask->getTaskTypeDes().c_str())+"完成";
        PlatformDepend::instance().showNotify(title,
                                              content->filename.c_str(),
                                              fellow->getIp().c_str());
    }
    else if (fileTask->getState()==FileTaskState::Error)
    {
        auto content = fileTask->getContent();
        auto fellow = fileTask->fellow();
        if (content == nullptr || fellow == nullptr)
            return;

        auto title = QString(fileTask->getTaskTypeDes().c_str())+"失败";
        auto file = QString(content->filename.c_str());
        file += "\n";
        file += fileTask->getDetailInfo().c_str();
        PlatformDepend::instance().showNotify(title,
                                              file,
                                              fellow->getIp().c_str());
    }

    if (mDownloadFileDlg->isVisible())
    {
        emit statChanged(fileTask);
    }
}

void MainWindow::onProgress(FileTask *fileTask)
{
    if (QThread::currentThread() != this->thread())
    {
        QMetaObject::invokeMethod(this, [this, fileTask](){
            onProgress(fileTask);
        }, Qt::QueuedConnection);
        return;
    }

    if (mDownloadFileDlg->isVisible())
    {
        emit progressChanged(fileTask);
    }
}

void MainWindow::onEvent(shared_ptr<ViewEvent> event)
{
    emit feiqViewEvent(event);
}

void MainWindow::onShowErrorAndQuit(const QString &text)
{
    QMessageBox::warning(this, "出错了，为什么？你猜！", text, QMessageBox::Close);

    QApplication::exit(-1);
}

void MainWindow::handleFeiqViewEvent(shared_ptr<ViewEvent> event)
{
    if (event->what == ViewEventType::FELLOW_UPDATE)
    {
        auto e = static_cast<FellowViewEvent*>(event.get());
        mFellowList.update(*(e->fellow.get()));
    }
    else if (event->what == ViewEventType::SEND_TIMEO || event->what == ViewEventType::MESSAGE)
    {
        //地球人都知道这个分支中的ViewEvent继承自FellowViewEvent
        auto e = static_cast<FellowViewEvent*>(event.get());
        auto fellow = e->fellow.get();

        if (isActiveWindow() && fellow == mRecvTextEdit->curFellow())
        {//窗口可见，处理当前用户消息，其他用户消息则放入通知队列
            readEvent(event.get());
        }
        else
        {//窗口不可见，放入未读队列并通知
            auto& umsg = addUnshownMessage(fellow, event);
            notifyUnshown(umsg);
            updateUnshownHint(fellow);
        }
    }
}

void MainWindow::refreshFellowList()
{
    if (mServer.isConnected())
    {
        mServer.reloadChannels();
        return;
    }
    mFeiq.sendImOnLine();
}

void MainWindow::addFellow()
{
    AddFellowDialog dlg(this);
    dlg.setServerMode(mServer.isConfigured());
    if (dlg.exec() == QDialog::Accepted)
    {
        auto ip = dlg.getIp().trimmed();
        if (mServer.isConfigured())
        {
            if (!mServer.isConnected())
            {
                mRecvTextEdit->addWarning("服务器尚未连接，请先连接远程服务器");
                return;
            }

            // 服务器模式按用户名添加并打开私聊
            const auto users = mServer.searchFellowByName(ip);
            if (!users.empty())
            {
                const Fellow *user = users.front();
                mServer.openDirectChannel(user->getServerUserId(),
                                          QString::fromStdString(user->getName()),
                                          QString::fromStdString(user->getServerUsername()));
                return;
            }

            mPendingServerAddUser = ip;
            mServer.searchUsers(ip);
        }
        else
        {
            userAddFellow(ip);
        }
    }
}

void MainWindow::openChooseEmojiDlg()
{
    mChooseEmojiDlg->exec();
}

void MainWindow::sendFiles(QList<QFileInfo> files)
{
    auto fellow = checkCurFellow();
    if (!fellow)
        return;

    for (auto file : files)
    {
        if (file.isFile())
        {
            sendFile(file.absoluteFilePath().toStdString());
        }
        else
        {
            mRecvTextEdit->addWarning("不支持发送："+file.absoluteFilePath());
        }
    }
}

void MainWindow::userAddFellow(QString ip)
{
    //创建好友
    auto fellow = make_shared<Fellow>();
    fellow->setIp(ip.toStdString());
    fellow->setOnLine(true);
    mFeiq.getModel().addFellow(fellow);

    //添加到列表
    auto& ref = *(fellow.get());
    mFellowList.update(ref);
    mFellowList.top(ref);

    //发送在线
    mFeiq.sendImOnLine(fellow->getIp());
}

void MainWindow::notifyUnshown(UnshownMessage& umsg)
{
    auto event = umsg.event.get();
    if (event->what == ViewEventType::SEND_TIMEO)
    {
        auto e = static_cast<const SendTimeoEvent*>(event);
        auto fellow = e->fellow.get();
        umsg.notifyId = showNotification(fellow, "发送超时:"+simpleTextOf(e->content.get()));
    }
    else if (event->what == ViewEventType::MESSAGE)
    {
        auto e = static_cast<const MessageViewEvent*>(event);
        auto fellow = e->fellow.get();
        QString text="";
        bool first=true;
        for (auto content : e->contents)
        {
            auto t = simpleTextOf(content.get());
            if (first)
            {
                text = t;
                first=false;
            }
            else
                text = text+"\n"+t;
        }
        umsg.notifyId = showNotification(fellow, text);
    }
}

long MainWindow::showNotification(const Fellow *fellow, const QString &text)
{
    if (fellow == nullptr)
        return 0;

    QString content(text);
    if (content.length()>100)
        content = content.left(100)+"...";
    return PlatformDepend::instance().showNotify(QString(fellow->getName().c_str())+":", content, fellow->getIp().c_str());
}

void MainWindow::navigateToFileTask(IdType packetNo, IdType fileId, bool upload)
{
    auto task = mFeiq.getModel().findTask(packetNo, fileId, upload ? FileTaskType::Upload : FileTaskType::Download);
    openDownloadDlg();
    mDownloadFileDlg->select(task.get());
}

void MainWindow::sendFile(std::string filepath)
{
    auto content = FileContent::createFileContentToSend(filepath);
    auto fellow = checkCurFellow();
    if (!fellow)
        return;

    if (content == nullptr)
    {
        mRecvTextEdit->addWarning("获取文件"+QString(filepath.c_str())+"的信息失败，不发送");
    }
    else
    {
        auto fileContent = shared_ptr<FileContent>(std::move(content));
        if (fellow->isRemote())
        {
            mRecvTextEdit->addWarning("服务器模式暂不支持文件传输，请切换局域网模式");
            return;
        }
        auto ret = mFeiq.send(fellow, fileContent);
        showResult(ret, fileContent.get());
    }
}

void MainWindow::sendFile()
{
    auto fellow = checkCurFellow();
    if (!fellow)
        return;

    //文件多选
    QFileDialog fdlg(this, "选择要发送的文件");
    fdlg.setFileMode(QFileDialog::ExistingFiles);
    if (fdlg.exec() == QDialog::Accepted)
    {
        auto list = fdlg.selectedFiles();
        auto count = list.count();
        for (int i = 0; i < count; i++)
        {
            auto path = list.at(i);
            sendFile(path.toStdString());
        }
    }
}

void MainWindow::sendKnock()
{
    auto fellow = checkCurFellow();
    if (fellow)
    {
        auto content = make_shared<KnockContent>();
        pair<bool, string> ret;
        if (fellow->isRemote())
        {
            const auto serverRet = mServer.sendKnock(fellow.get());
            ret = make_pair(serverRet.first, serverRet.second.toStdString());
        }
        else
        {
            ret = mFeiq.send(fellow, content);
        }
        showResult(ret, content.get());
    }
}

void MainWindow::sendText()
{
    auto text = mSendTextEdit->toPlainText();
    if (text.isEmpty())
    {
        mRecvTextEdit->addWarning("发送空文本是不科学的，驳回");
        return;
    }


    auto fellow = checkCurFellow();
    if (fellow)
    {
        auto content = make_shared<TextContent>();
        content->text = text.toStdString();
        pair<bool, string> ret;
        if (fellow->isRemote())
        {
            const auto serverRet = mServer.sendText(fellow.get(), text);
            ret = make_pair(serverRet.first, serverRet.second.toStdString());
        }
        else
        {
            ret = mFeiq.send(fellow, content);
        }
        showResult(ret, content.get());
        mSendTextEdit->clear();
    }
}

void MainWindow::finishSearch(const Fellow *fellow)
{
    if (fellow && fellow->isRemote())
    {
        const long remoteUserId = fellow->getServerUserId();
        if (remoteUserId > 0)
        {
            mServer.openDirectChannel(remoteUserId,
                                      QString::fromStdString(fellow->getName()),
                                      QString::fromStdString(fellow->getServerUsername()));
            return;
        }
    }
    mFellowList.top(*fellow);
    openChartTo(fellow);
}

void MainWindow::openSettings()
{
    ServerSettingsDialog dlg(mSettings, &mServer, this);
    connect(&dlg, &QDialog::accepted, this, &MainWindow::initServer);
    dlg.exec();
}

void MainWindow::openSearchDlg()
{
    mSearchFellowDlg->exec();
}

void MainWindow::openDownloadDlg()
{
    mDownloadFileDlg->show();
    mDownloadFileDlg->raise();
}

void MainWindow::initServer()
{
    mServer.setView(this);
    const bool enabled = mSettings->value("server/enabled", false).toBool();
    if (enabled)
    {
        mServer.start(mSettings->value("server/url").toString(),
                      mSettings->value("server/username").toString(),
                      mSettings->value("server/password").toString());
    }
    else
    {
        mServer.stop();
        onServerStateChanged(ServerEngine::State::Disconnected, "服务器未启用");
    }
}

void MainWindow::disconnectServer()
{
    mServer.stop();
    onServerStateChanged(ServerEngine::State::Disconnected, "已断开服务器");
}

void MainWindow::onServerStateChanged(ServerEngine::State state, const QString &detail)
{
    if (!mServerStatusLabel)
        return;

    switch (state)
    {
    case ServerEngine::State::Connecting:
        mServerStatusLabel->setText("正在连接服务器...");
        mServerStatusLabel->setStyleSheet("color: #b45309;");
        ui->actionConnectServer->setEnabled(false);
        ui->actionDisconnectServer->setEnabled(true);
        break;
    case ServerEngine::State::Connected:
        mServerStatusLabel->setText("已连接服务器");
        mServerStatusLabel->setStyleSheet("color: #15803d;");
        ui->actionConnectServer->setEnabled(false);
        ui->actionDisconnectServer->setEnabled(true);
        break;
    case ServerEngine::State::Disconnected:
        mServerStatusLabel->setText("服务器未连接" + (detail.isEmpty() ? QString() : "：" + detail));
        mServerStatusLabel->setStyleSheet("color: #b91c1c;");
        ui->actionConnectServer->setEnabled(true);
        ui->actionDisconnectServer->setEnabled(false);
        break;
    }
}

void MainWindow::loadMoreHistory()
{
    auto fellow = checkCurFellow();
    if (!fellow)
        return;
    if (fellow->isRemote())
        mServer.loadMoreHistory(fellow.get());
    else
        mRecvTextEdit->addWarning("局域网模式没有更多历史消息");
}

vector<const Fellow *> MainWindow::fellowSearchDriver(const QString &text)
{
    auto fellows = mFeiq.getModel().searchFellow(text.toStdString());
    vector<const Fellow*> result;
    for (auto fellow : fellows)
    {
        result.push_back(fellow.get());
    }
    if (mServer.isConnected())
    {
        const auto serverFellows = mServer.searchChannels(text);
        result.insert(result.end(), serverFellows.begin(), serverFellows.end());
    }
    return result;
}

void MainWindow::initFeiq()
{
    //配置我Q
    auto name = mSettings->value("user/name").toString();
    if (name.isEmpty())
    {
        if (!mSettings->value("server/enabled", false).toBool())
            mRecvTextEdit->addWarning("未设置用户名，暂时使用默认名称");
        name = mSettings->value("server/username", "我Q用户").toString();
    }

    mFeiq.setMyName(name.toStdString());
    mFeiq.setMyHost(mSettings->value("user/host","WorkQ").toString().toStdString());

    auto customGrroup = mSettings->value("network/custom_group", "").toString();
    if (!customGrroup.isEmpty())
    {
        auto list = customGrroup.split("|");
        for (int i = 0; i < list.size(); i++)
        {
            QString ipPrefix = list[i];
            if (ipPrefix.endsWith("."))
            {
                for (int j = 2; j < 254; j++)
                {
                    auto ip = ipPrefix+QString::number(j);
                    mFeiq.addToBroadcast(ip.toStdString());
                }
            }
        }
    }

    mFeiq.setView(this);

    mFeiq.enableIntervalDetect(60);

    //启动我Q
    auto ret = mFeiq.start();
    if (!ret.first)
    {
        emit showErrorAndQuit(ret.second.c_str());
    }

    qDebug()<<"WorkQ started";
    initServer();
}

void MainWindow::updateUnshownHint(const Fellow *fellow)
{
    auto it = mUnshownEvents.find(fellow);
    if (it != mUnshownEvents.end())
    {
        auto count = it->second.size();
        if (count == 0)
        {
            mFellowList.mark(*fellow, "");
        }
        else
        {
            mFellowList.mark(*fellow, QString::number(count));
        }
    }

    setBadgeNumber(getUnreadCount());
}

int MainWindow::getUnreadCount()
{
    auto begin = mUnshownEvents.begin();
    auto end = mUnshownEvents.end();
    auto count = 0;
    for (auto it = begin; it != end; it++)
    {
        for (auto msg : it->second)
        {
            if (msg.isUnread())
                ++count;
        }
    }
    return count;
}

void MainWindow::flushUnshown(const Fellow *fellow)
{
    auto it = mUnshownEvents.find(fellow);
    if (it != mUnshownEvents.end())
    {
        auto& list = (*it).second;
        while (!list.empty())
        {
            auto msg = list.front();
            readEvent(msg.event.get());
            list.pop_front();
        }
    }
}

void MainWindow::readEvent(const ViewEvent *event)
{
    if (event->what == ViewEventType::SEND_TIMEO)
    {
        auto e = static_cast<const SendTimeoEvent*>(event);
        auto simpleText = simpleTextOf(e->content.get());
        if (simpleText.length()>20){
            simpleText = simpleText.left(20)+"...";
        }
        mRecvTextEdit->addWarning("发送超时:"+simpleText);
    }
    else if (event->what == ViewEventType::MESSAGE)
    {
        auto e = static_cast<const MessageViewEvent*>(event);
        auto time =  e->when.time_since_epoch().count();
        for (auto content : e->contents)
        {
            if (e->fellow == nullptr)
                mRecvTextEdit->addMyContent(content.get(), time);
            else
                mRecvTextEdit->addFellowContent(content.get(), time);
        }
    }
}

void MainWindow::setBadgeNumber(int number)
{
    PlatformDepend::instance().setBadgeNumber(number);
}

QString MainWindow::simpleTextOf(const Content *content)
{
    if (content == nullptr)
        return "";

    switch (content->type()) {
    case ContentType::Text:
        return static_cast<const TextContent*>(content)->text.c_str();
        break;
    case ContentType::File:
        return static_cast<const FileContent*>(content)->filename.c_str();
    case ContentType::Knock:
        return "窗口抖动";
    default:
        return "***";
        break;
    }
}

UnshownMessage &MainWindow::addUnshownMessage(const Fellow *fellow, shared_ptr<ViewEvent> event)
{
    UnshownMessage msg;
    msg.event = event;
    mUnshownEvents[fellow].push_back(msg);
    return mUnshownEvents[fellow].back();
}

UnshownMessage* MainWindow::findUnshownMessage(int id)
{
    auto begin = mUnshownEvents.begin();
    auto end = mUnshownEvents.end();

    for (auto it = begin; it != end; it++)
    {
        for (auto& msg : it->second){
            if (msg.notifyId == id)
                return &msg;
        }
    }

    return nullptr;
}
