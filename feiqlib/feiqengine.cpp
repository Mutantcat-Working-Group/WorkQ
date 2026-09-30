#include "feiqengine.h"
#include "protocol.h"
#include "ipmsg.h"
#include <memory>
#include "utils.h"
#include <fstream>
#include "defer.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <QFile>
#include <QString>

class ContentSender : public SendProtocol
{
public:
    void setContent(const Content* content)
    {
        mContent = content;
    }

protected:
    const Content* mContent;
};

class SendTextContent : public ContentSender
{
public:
    int cmdId() override{return IPMSG_SENDMSG|IPMSG_SENDCHECKOPT;}
    void write(ostream& os) override
    {
        auto content = static_cast<const TextContent*>(mContent);
        if (content->format.empty())
        {
            os<<encOut->convert(content->text);
        }
        else
        {
            os<<encOut->convert(content->text)
             <<"{"
            <<encOut->convert(content->format)
            <<"}";
        }
    }
};

class SendKnockContent : public ContentSender
{
public:
    int cmdId() override{return IPMSG_KNOCK;}
    void write(ostream &os) override {(void)os;}
};

class SendFileContent : public ContentSender
{
public:
    int cmdId() override {return IPMSG_SENDMSG|IPMSG_FILEATTACHOPT;}
    void write(ostream& os) override
    {
        auto content = static_cast<const FileContent*>(mContent);
        char sep = HLIST_ENTRY_SEPARATOR;
        auto filename = content->filename;
        stringReplace(filename, ":", "::");//估摸着协议不会变，偷懒下
        os<<(char)0
          <<to_string(content->fileId)
         <<sep
        <<encOut->convert(filename)
        <<sep
        <<std::hex<<content->size
        <<sep
        <<content->modifyTime
        <<sep
        <<content->fileType
        <<sep
        <<FILELIST_SEPARATOR;
    }
};

class SendImOnLine : public SendProtocol
{
public:
    SendImOnLine(const string& name):mName(name){}
    int cmdId() override{return IPMSG_BR_ENTRY;}
    void write(ostream &os) override
    {
        os<<encOut->convert(mName);
    }

private:
    string mName;
};

class SendImOffLine : public SendProtocol
{
public:
    SendImOffLine(const string& name):mName(name){}
    int cmdId() override {return IPMSG_BR_EXIT;}
    void write(ostream &os) override
    {
        os<<encOut->convert(mName);
    }
private:
    string mName;
};

/**
 * @brief The AnsSendCheck class 发送消息我收到了
 */
class SendSentCheck : public SendProtocol
{
public:
    SendSentCheck(const string& packetNo)
        :mPacketNo(packetNo){}

    int cmdId() override{return IPMSG_RECVMSG;}

    void write(ostream& os) override
    {
        os<<mPacketNo;
    }
private:
    string mPacketNo;
};

/**
 * @brief The SendReadCheck class 发送消息我已经读了
 */
class SendReadCheck : public SendProtocol
{
public:
    SendReadCheck(const string& packetNo)
        :mPacketNo(packetNo){}
public:
    int cmdId() override {return IPMSG_READMSG;}
    void write(ostream& os) override
    {
        os<<mPacketNo;
    }
private:
    string mPacketNo;
};

/**
 * @brief The AnsBrEntry class 回复好友上线包
 */
class AnsBrEntry : public SendProtocol
{
public:
    AnsBrEntry(const string& myName):mName(myName){}
public:
    int cmdId() override { return IPMSG_ANSENTRY;}
    void write(ostream &os) override {
        os<<encOut->convert(mName);
    }
private:
    const string& mName;
};

//定义触发器
typedef std::function<void (shared_ptr<Post> post)> OnPostReady;
#define DECLARE_TRIGGER(name)\
    public:\
    name(OnPostReady trigger) : mTrigger(trigger){}\
    private:\
    OnPostReady mTrigger;\
    void trigger(shared_ptr<Post> post){mTrigger(post);}

/**
 * @brief The RecvAnsEntry class 好友响应我们的上线消息
 */
class RecvAnsEntry : public RecvProtocol
{
    DECLARE_TRIGGER(RecvAnsEntry)
public:
    bool read(shared_ptr<Post> post)
    {
        if (IS_CMD_SET(post->cmdId, IPMSG_ANSENTRY))
        {
            auto converted = toString(encIn->convert(post->extra));
            post->from->setName(converted);
            trigger(post);
            return true;
        }
        return false;
    }
};
/**
 * @brief The RecvBrEntry class 好友上线
 */
class RecvBrEntry : public RecvProtocol
{
    DECLARE_TRIGGER(RecvBrEntry)
public:
    bool read(shared_ptr<Post> post)
    {
        if (IS_CMD_SET(post->cmdId, IPMSG_BR_ENTRY))
        {
            post->from->setName(toString(encIn->convert(post->extra)));
            trigger(post);
            return true;
        }
        return false;
    }
};
/**
 * @brief The RecvBrExit class 好友下线
 */
class RecvBrExit : public RecvProtocol
{
    DECLARE_TRIGGER(RecvBrExit)
public:
    bool read(shared_ptr<Post> post)
    {
        if (IS_CMD_SET(post->cmdId, IPMSG_BR_EXIT))
        {
            post->from->setOnLine(false);
            trigger(post);
            return true;
        }
        return false;
    }
};
/**
 * @brief The RecvKnock class  窗口抖动
 */
class RecvKnock : public RecvProtocol
{
public:
    bool read(shared_ptr<Post> post)
    {
        if (IS_CMD_SET(post->cmdId, IPMSG_KNOCK))
        {
            post->contents.push_back(make_shared<KnockContent>());
        }
        return false;
    }
};
/**
 * @brief The AnsSendCheck class
 */
class RecvSendCheck : public RecvProtocol
{
    DECLARE_TRIGGER(RecvSendCheck)
public:
    bool read(shared_ptr<Post> post)
    {
        if (IS_OPT_SET(post->cmdId, IPMSG_SENDCHECKOPT))
            trigger(post);
        return false;
    }
};

/**
 * @brief The RecvReadCheck class 接收到请求阅后通知
 */
class RecvReadCheck : public RecvProtocol
{
    DECLARE_TRIGGER(RecvReadCheck)
public:
    bool read(shared_ptr<Post> post)
    {
        if (IS_OPT_SET(post->cmdId, IPMSG_READCHECKOPT))
            trigger(post);
        return false;
    }
};

/**
 * @brief The RecvText class 接收文本消息
 */
class RecvText : public RecvProtocol
{
public:
    bool read(shared_ptr<Post> post)
    {
        if (!IS_CMD_SET(post->cmdId, IPMSG_SENDMSG))
            return false;

        auto& extra = post->extra;

        auto end = extra.end();
        auto begin = extra.begin();
        auto found = std::find(begin, end, 0);
        if (found != begin)//有找到0，且不是第一个字符
        {
            string rawText;
            rawText.assign(begin, found);

            auto content = createTextContent(encIn->convert(rawText));
            post->contents.push_back(shared_ptr<Content>(std::move(content)));
        }

        return false;
    }
private:
    unique_ptr<TextContent> createTextContent(const string& raw)
    {
        auto content = unique_ptr<TextContent>(new TextContent());
        auto begin = raw.find('{');
        auto end = raw.find("}", begin+1);

        if (begin != raw.npos && end != raw.npos)
        {
            content->text = raw.substr(0, begin);
            content->format = raw.substr(begin+1, end-begin-1);
        }
        else
        {
            content->text = raw;
        }
        return content;
    }
};

class RecvFile : public RecvProtocol
{
public:
    bool read(shared_ptr<Post> post)
    {
        if (!IS_OPT_SET(post->cmdId, IPMSG_FILEATTACHOPT) || !IS_CMD_SET(post->cmdId, IPMSG_SENDMSG))
            return false;

        //文件任务信息紧随文本消息之后，中间相隔一个ascii 0
        //一个文件任务信息格式为fileId:filename:fileSize:modifyTime:fileType:其他扩展属性
        //多个文件任务以ascii 7分割
        //文件名含:，以::表示
        auto& extra = post->extra;
        auto end = extra.end();
        auto found = find(extra.begin(), end, 0);
        if (found == end)
            return false;
        ++found;

        while (found != end)
        {
            auto endTask = find(found, end, FILELIST_SEPARATOR);
            if (endTask == end)
                break;

            auto content = createFileContent(found, endTask);
            if (content != nullptr)
            {
                content->packetNo = safeParse<IdType>(post->packetNo, 0);
                post->contents.push_back(shared_ptr<Content>(std::move(content)));
            }

            found = ++endTask;
        }

        return false;
    }
private:
    unique_ptr<FileContent> createFileContent(vector<char>::iterator from,
                                          vector<char>::iterator to)
    {
        unique_ptr<FileContent> content(new FileContent());

        auto values = splitAllowSeperator(from, to, HLIST_ENTRY_SEPARATOR);
        const int fieldCount = 5;
        if (values.size() < fieldCount)
            return nullptr;

        content->fileId = safeParse<IdType>(values[0], 0);
        content->filename = encIn->convert(values[1]);
        content->size = safeParse<long long>(values[2],0,16);
        content->modifyTime = safeParse<long long>(values[3],0,16);
        content->fileType = safeParse<int>(values[4],0,16);

        return content;
    }
};

class Debuger : public RecvProtocol
{
public:
    bool read(shared_ptr<Post> post)
    {
        cout<<"==========================="<<endl;
        cout<<"cmd id : "<<std::hex<<post->cmdId<<endl;
        cout<<"from: "<<post->from->toString()<<endl;
        int count = 0;

        for (unsigned char ch : post->extra){
            cout<<setw(2)<<setfill('0')<<hex<<(unsigned int)ch<<" ";
            if (++count >= 8){
                cout<<endl;
                count=0;
            }
        }
        cout<<endl;
        return false;
    }
};

class RecvReadMessage : public RecvProtocol
{
    DECLARE_TRIGGER(RecvReadMessage)
public:
    bool read(shared_ptr<Post> post)
    {
        if (post->cmdId == IPMSG_RECVMSG)
        {
            IdType id = static_cast<IdType>(safeParse<long long>(toString(post->extra), 0));
            auto content = make_shared<IdContent>();
            content->id = id;
            post->addContent(content);
            trigger(post);
            return true;
        }
        return false;
    }
};

class RecvImage : public RecvProtocol
{
public:
    bool read(shared_ptr<Post> post)
    {
        if (IS_CMD_SET(post->cmdId, IPMSG_SENDIMAGE)
            && IS_OPT_SET(post->cmdId, IPMSG_FILEATTACHOPT))
        {
            if (post->extra.size() < 8)
                return false;

            char id[9]={0};
            memcpy(id, post->extra.data(), 8);
            auto content = make_shared<ImageContent>();
            content->id = id;
            post->contents.push_back(content);
        }
        return false;
    }
};

/**
 * @brief The EndRecv class 终止解析链
 */
class EndRecv : public RecvProtocol
{
    DECLARE_TRIGGER(EndRecv)
public:
    bool read(shared_ptr<Post> post)
    {
        if (!post->contents.empty())
            trigger(post);
        return true;
    }
};

//添加一条接收协议，触发时更新好友信息，并调用func
#define ADD_RECV_PROTOCOL(protocol, func)\
{\
    RecvProtocol* p = new protocol([this](shared_ptr<Post> post){\
        post->from = this->addOrUpdateFellow(post->from);\
        this->func(post);});\
    mRecvProtocols.push_back(unique_ptr<RecvProtocol>(p));\
    mCommu.addRecvProtocol(p);\
 }

//添加一条接收协议，无触发
#define ADD_RECV_PROTOCOL2(protocol)\
{\
    RecvProtocol* p = new protocol();\
    mRecvProtocols.push_back(unique_ptr<RecvProtocol>(p));\
    mCommu.addRecvProtocol(p);\
}

//添加一条接收协议，触发时更新好友信息
#define ADD_RECV_PROTOCOL3(protocol)\
{\
    RecvProtocol* p = new protocol([this](shared_ptr<Post> post){\
        post->from = this->addOrUpdateFellow(post->from);});\
    mRecvProtocols.push_back(unique_ptr<RecvProtocol>(p));\
    mCommu.addRecvProtocol(p);\
}

//添加一条发送协议
#define ADD_SEND_PROTOCOL(protocol, sender, args...)\
{\
    mContentSender[protocol]=make_shared<sender>(##args);\
}

FeiqEngine::FeiqEngine()
{
    ADD_RECV_PROTOCOL2(Debuger);//仅用于开发中的调试

    ADD_RECV_PROTOCOL3(RecvAnsEntry);
    ADD_RECV_PROTOCOL(RecvBrEntry, onBrEntry);
    ADD_RECV_PROTOCOL3(RecvBrExit);
    ADD_RECV_PROTOCOL(RecvSendCheck, onSendCheck);
    ADD_RECV_PROTOCOL(RecvReadCheck, onReadCheck);
    ADD_RECV_PROTOCOL(RecvReadMessage, onReadMessage);//好友回复消息已经阅读
    ADD_RECV_PROTOCOL2(RecvText);
    ADD_RECV_PROTOCOL2(RecvImage);
    ADD_RECV_PROTOCOL2(RecvKnock);
    ADD_RECV_PROTOCOL2(RecvFile);
    ADD_RECV_PROTOCOL(EndRecv, onMsg);

    ADD_SEND_PROTOCOL(ContentType::Text, SendTextContent);
    ADD_SEND_PROTOCOL(ContentType::Knock, SendKnockContent);
    ADD_SEND_PROTOCOL(ContentType::File, SendFileContent);

    mCommu.setFileServerHandler(std::bind(&FeiqEngine::fileServerHandler,
                                          this,
                                          placeholders::_1,
                                          placeholders::_2,
                                          placeholders::_3,
                                          placeholders::_4));
}

FeiqEngine::~FeiqEngine()
{
    stop();
}

pair<bool, string> FeiqEngine::send(shared_ptr<Fellow> fellow, shared_ptr<Content> content)
{
    if (content == nullptr)
        return {false, "要发送的内容无效"};
    if (fellow == nullptr)
        return {false, "发送对象无效"};

    lock_guard<mutex> lock(mSendMutex);
    auto& sender = mContentSender[content->type()];
    if (sender == nullptr)
        return {false, "no send protocol can send"};

    sender->setContent(content.get());
    auto ip = fellow->getIp();
    auto ret = mCommu.send(ip, *sender);
    if (ret.first == 0)
    {
        return {false, ret.second};
    }

    content->setPacketNo(ret.first);

    if (content->type() == ContentType::File){
        auto ptr = dynamic_pointer_cast<FileContent>(content);
        if (ptr != nullptr)
            mModel.addUploadTask(fellow, ptr)->setObserver(mView);
    }
    else if (content->type() == ContentType::Text){
        auto handler = std::bind(&FeiqEngine::onSendTimeo, this, placeholders::_1, ip, content);
        mAsyncWait.addWaitPack(content->packetNo, handler, 5000);
    }
    return {true, ""};
}

pair<bool, string> FeiqEngine::sendFiles(shared_ptr<Fellow> fellow, list<shared_ptr<FileContent>> &files)
{
    for (auto file : files) {
        auto ret = send(fellow, file);
        if (!ret.first)
            return ret;
    }
    return {true,""};
}

bool FeiqEngine::downloadFile(FileTask* task)
{
    if (task==nullptr)
        return false;

    auto content = task->getContent();
    auto fellow = task->fellow();
    if (content == nullptr || fellow == nullptr)
        return false;

    auto sharedTask = mModel.findTask(content->packetNo,
                                      content->fileId,
                                      FileTaskType::Download);
    if (!sharedTask)
        sharedTask = mModel.addDownloadTask(fellow, content);
    sharedTask->setObserver(mView);

    auto client = mCommu.requestFileData(fellow->getIp(), *content, 0);
    if (client == nullptr)
    {
        sharedTask->setState(FileTaskState::Error, "请求下载文件失败，可能好友已经取消");
        return false;
    }

    auto func = [sharedTask, client = std::move(client)]() mutable {
        auto content = sharedTask->getContent();
        QFile of(QString::fromStdString(content->path));
        if (!of.open(QIODevice::WriteOnly)){
            sharedTask->setState(FileTaskState::Error, "无法打开文件进行保存");
            return;
        }

        const int unitSize = 2048;//一次请求2k
        const int maxTimeoCnt = 3;//最多允许超时3次
        const int timeo = 2000;//允许超时2s

        long long recv = 0;
        auto total = content->size;
        std::array<char, unitSize> buf;
        int timeoCnt = 0;
        sharedTask->setState(FileTaskState::Running);
        while (recv < total)
        {
            if (sharedTask->hasCancelPending())
            {
                sharedTask->setState(FileTaskState::Canceled);
                of.close();
                return;
            }

            auto left = total - recv;
            auto request = unitSize > left ? left : unitSize;
            auto got = client->recv(buf.data(), request, timeo);
            if (got == -1 && ++timeoCnt >= maxTimeoCnt)
            {
                sharedTask->setState(FileTaskState::Error, "下载文件超时，好友可能掉线");
                of.close();
                return;
            }
            else if (got <= 0)
            {
                sharedTask->setState(FileTaskState::Error, "接收数据出错，可能网络错误");
                of.close();
                return;
            }
            else
            {
                of.write(buf.data(), got);
                recv+=got;
                sharedTask->setProcess(recv);
            }
        }

        of.close();
        sharedTask->setProcess(total);
        sharedTask->setState(FileTaskState::Finish);
    };

    trackThread(std::thread(func));
    return true;
}

class GetPubKey : public SendProtocol
{
public:
    int cmdId() {return IPMSG_GETPUBKEY;}
    void write(ostream& os){
        (void)os;
    }
};

pair<bool, string> FeiqEngine::start()
{
    if (mStarted)
    {
        return {true, "已经启动过"};
    }

    mCommu.setMyHost(encOut->convert(mHost));
    mCommu.setMyName(encOut->convert(mName));
    auto result = mCommu.start();

    if(result.first)
    {
        mAsyncWait.start();

        mMsgThd.setHandler(std::bind(&FeiqEngine::dispatchMsg, this, placeholders::_1));
        mMsgThd.start();

        mStarted = true;
        sendImOnLine();
    }

    return result;
}

void FeiqEngine::stop()
{
    bool wasStarted = mStarted.exchange(false);

    //即使引擎从未成功启动，也要回收 enableIntervalDetect 创建的线程
    {
        lock_guard<mutex> lock(mIntervalMutex);
        ++mIntervalGen;
        mIntervalCv.notify_all();
    }
    if (mIntervalThread.joinable())
        mIntervalThread.join();

    if (wasStarted)
    {
        SendImOffLine imOffLine(mName);
        mCommu.send("255.255.255.255", imOffLine);
        broadcastToCurstomGroup(imOffLine);
    }

    mCommu.stop();
    mAsyncWait.stop();
    mMsgThd.stop();

    lock_guard<mutex> lock(mThreadsMutex);
    for (auto& thd : mFileThreads)
        if (thd.joinable())
            thd.join();
    mFileThreads.clear();
}

void FeiqEngine::addToBroadcast(const string &ip)
{
    mBroadcast.push_back(ip);
}

void FeiqEngine::setMyHost(string host)
{
    mHost=host;
    if (mName.empty())
        mName = mHost;
}

void FeiqEngine::setMyName(string name){
    mName=name;
    if (mName.empty())
        mName = mHost;
}

void FeiqEngine::sendImOnLine(const string &ip)
{
    SendImOnLine imOnLine(mName);

    if (ip.empty())
    {
        mCommu.send("255.255.255.255", imOnLine);
        broadcastToCurstomGroup(imOnLine);
    }
    else
    {
        mCommu.send(ip, imOnLine);
    }
}

void FeiqEngine::enableIntervalDetect(int seconds)
{
    //把需要周期发送的目标抓下来，后台线程不再访问会被 stop 修改的成员
    auto name = mName;
    auto targets = mBroadcast;

    //通过代际计数让旧线程立即退出，避免 join 等到下一轮发送
    {
        lock_guard<mutex> lock(mIntervalMutex);
        ++mIntervalGen;
        mIntervalCv.notify_all();
    }
    if (mIntervalThread.joinable())
        mIntervalThread.join();

    mIntervalThread = std::thread([this, seconds, name, targets](){
        auto gen = mIntervalGen;
        while (true)
        {
            unique_lock<mutex> lock(mIntervalMutex);
            mIntervalCv.wait_for(lock, std::chrono::seconds(seconds), [this, gen](){
                return !mStarted || mIntervalGen != gen;
            });
            if (!mStarted || mIntervalGen != gen)
                break;
            lock.unlock();

            SendImOnLine imOnLine(name);
            for (auto ip : targets)
                mCommu.send(ip, imOnLine);
        }
    });
}


FeiqModel &FeiqEngine::getModel()
{
    return mModel;
}

void FeiqEngine::onBrEntry(shared_ptr<Post> post)
{
    AnsBrEntry ans(mName);
    mCommu.send(post->from->getIp(), ans);
}

void FeiqEngine::onMsg(shared_ptr<Post> post)
{
    static vector<string> rejectedImages;

    auto event = make_shared<MessageViewEvent>();
    event->when = post->when;
    event->fellow = post->from;

    auto it = post->contents.begin();
    auto end = post->contents.end();

    string reply;
    while (it != end)//过滤消息内容：删除不支持的包，并回复好友
    {
        bool rejected = false;
        if ((*it)->type() == ContentType::File)
        {
            auto fc = static_pointer_cast<FileContent>(*it);

            if (fc->fileType == IPMSG_FILE_REGULAR)//TODO:与飞秋的文件夹传输协议还没支持
                mModel.addDownloadTask(event->fellow, fc);
            else if (fc->fileType == IPMSG_FILE_DIR)
            {
                rejected=true;
                reply+="我Q还不支持接收目录："+fc->filename+"\n";
            }
        }
        else if ((*it)->type() == ContentType::Text)
        {
            auto tc = static_cast<TextContent*>((*it).get());
            string begin = "/~#>";
            string end = "<B~";
            if (startsWith(tc->text, begin) && endsWith(tc->text, end))
            {
                rejected=true;
            }
        }
        else if ((*it)->type() == ContentType::Image)
        {
            //这个包还没被拒绝过，发送拒绝消息
            auto ic = static_cast<ImageContent*>((*it).get());
            if (std::find(rejectedImages.begin(), rejectedImages.end(), ic->id)==rejectedImages.end())
            {
                reply+="我Q还不支持接收图片，请用文件形式发送图片\n";
                rejectedImages.push_back(ic->id);
            }
            rejected=true;
        }

        if (!rejected)
        {
            event->contents.push_back(*it);
        }
        ++it;
    }

    if (!reply.empty())
    {
        SendTextContent send;
        TextContent content;
        content.text = reply;
        send.setContent(&content);
        mCommu.send(post->from->getIp(), send);
    }

    if (!event->contents.empty())
        mMsgThd.sendMessage(event);
}

void FeiqEngine::onSendCheck(shared_ptr<Post> post)
{
    SendSentCheck reply(post->packetNo);
    mCommu.send(post->from->getIp(), reply);
}

void FeiqEngine::onReadCheck(shared_ptr<Post> post)
{
    SendReadCheck reply(post->packetNo);
    mCommu.send(post->from->getIp(), reply);
}

void FeiqEngine::onSendTimeo(IdType packetId, const string& ip, shared_ptr<Content> content)
{
    auto event = make_shared<SendTimeoEvent>();
    event->fellow = mModel.findFirstFellowOf(ip);
    if (event->fellow == nullptr)
        return;

    event->content = content;
    mMsgThd.sendMessage(event);
}

void FeiqEngine::onReadMessage(shared_ptr<Post> post)
{
    if (post->contents.empty())
        return;
    auto content = dynamic_pointer_cast<IdContent>(post->contents[0]);
    if (content == nullptr)
        return;
    mAsyncWait.clearWaitPack(content->id);
}

void FeiqEngine::fileServerHandler(unique_ptr<TcpSocket> client, int packetNo, int fileId, int offset)
{
    auto task = mModel.findTask(packetNo, fileId);
    if (task == nullptr || task->getContent() == nullptr)
        return;

    auto func = [task, offset](unique_ptr<TcpSocket> client){
        QFile is(QString::fromStdString(task->getContent()->path));
        if (!is.open(QIODevice::ReadOnly))
        {
            task->setState(FileTaskState::Error, "无法读取文件");
            return;
        }

        if (offset > 0)
            is.seek(offset);

        const int unitSize = 2048;//一次发送2k
        std::array<char, unitSize> buf;
        auto total = task->getContent()->size;
        long long sent = 0;

        task->setState(FileTaskState::Running);
        while (sent < total && !is.atEnd())
        {
            auto left = total - sent;
            auto request = unitSize > left ? left : unitSize;
            int got = is.read(buf.data(), request);
            if (got <= 0)
            {
                task->setState(FileTaskState::Error, "读取文件出错");
                is.close();
                return;
            }
            got = client->send(buf.data(), got);
            if (got < 0)
            {
                task->setState(FileTaskState::Error, "无法发送数据，可能是网络问题");
                is.close();
                return;
            }

            sent+=got;
            task->setProcess(sent);
        }

        if (sent != total)
        {
            task->setState(FileTaskState::Error, "文件未完整发送，可能是发送期间文件被改动");
        }
        else
        {
            task->setProcess(total);
            task->setState(FileTaskState::Finish);
        }

        is.close();
    };

    trackThread(std::thread(func, std::move(client)));
}

void FeiqEngine::trackThread(std::thread thd)
{
    lock_guard<mutex> lock(mThreadsMutex);
    mFileThreads.push_back(std::move(thd));
}

shared_ptr<Fellow> FeiqEngine::addOrUpdateFellow(shared_ptr<Fellow> fellow)
{
    auto f = mModel.getFullInfoOf(fellow);
    bool shouldApdate = false;

    if (f == nullptr)
    {
        mModel.addFellow(fellow);
        f = fellow;
        shouldApdate = true;
    }
    else
    {
        if (f->update(*fellow))
            shouldApdate = true;
    }

    if (shouldApdate){
        auto event = make_shared<FellowViewEvent>();
        event->what = ViewEventType::FELLOW_UPDATE;
        event->fellow = f;
        event->when = Post::now();
        mMsgThd.sendMessage(event);
    }

    return f;
}

void FeiqEngine::dispatchMsg(shared_ptr<ViewEvent> msg)
{
    if (mView)
        mView->onEvent(msg);
}

void FeiqEngine::broadcastToCurstomGroup(SendProtocol &protocol)
{
    for (auto ip : mBroadcast)
    {
        if (!mStarted)
            break;//发送过程是一个耗时网络操作，如果已经stop，则中断

        mCommu.send(ip, protocol);
    }
}
