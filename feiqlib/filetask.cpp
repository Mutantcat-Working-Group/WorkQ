#include "filetask.h"

FileTask::FileTask()
{

}

FileTask::FileTask(shared_ptr<FileContent> fileContent, FileTaskType type)
    :mContent(fileContent), mType(type)
{
}

void FileTask::setObserver(IFileTaskObserver *observer)
{
    lock_guard<mutex> lock(mStateMutex);
    mObserver = observer;
}

void FileTask::setProcess(long long val)
{
    long long total = mContent ? mContent->size : 0;
    const int minNotifySize = 102400;//至少变化了100k才通知
    auto notifySize = total > 0 ? total/100 : minNotifySize;//每1%通知一次
    if (notifySize < minNotifySize)
        notifySize = minNotifySize;

    bool shouldNotify = false;
    {
        lock_guard<mutex> lock(mStateMutex);
        mProcess = val;
        if (mProcess - mLastProcess >= notifySize || mProcess >= total)
        {
            mLastProcess = mProcess;
            shouldNotify = true;
        }
    }

    IFileTaskObserver* observer = nullptr;
    {
        lock_guard<mutex> lock(mStateMutex);
        observer = mObserver;
    }

    if (shouldNotify && observer)
        observer->onProgress(this);
}

void FileTask::setState(FileTaskState val, const string &msg)
{
    IFileTaskObserver* observer = nullptr;
    {
        lock_guard<mutex> lock(mStateMutex);
        mState = val;
        mMsg = msg;
        observer = mObserver;
    }

    if (observer)
        observer->onStateChanged(this);
}

void FileTask::setFellow(shared_ptr<Fellow> fellow)
{
    lock_guard<mutex> lock(mStateMutex);
    mFellow = fellow;
}

void FileTask::cancel()
{
    lock_guard<mutex> lock(mStateMutex);
    mCancelPending=true;
}

bool FileTask::hasCancelPending()
{
    lock_guard<mutex> lock(mStateMutex);
    return mCancelPending;
}

shared_ptr<Fellow> FileTask::fellow() const
{
    lock_guard<mutex> lock(mStateMutex);
    return mFellow;
}

long long FileTask::getProcess() const
{
    lock_guard<mutex> lock(mStateMutex);
    return mProcess;
}

FileTaskState FileTask::getState() const
{
    lock_guard<mutex> lock(mStateMutex);
    return mState;
}

string FileTask::getDetailInfo() const
{
    lock_guard<mutex> lock(mStateMutex);
    return mMsg;
}

shared_ptr<FileContent> FileTask::getContent() const
{
    return mContent;
}

FileTaskType FileTask::type() const
{
    return mType;
}

string FileTask::getTaskTypeDes() const
{
    if (mType == FileTaskType::Upload)
        return "发送";
    else if (mType == FileTaskType::Download)
        return "接收";
    return "";
}
