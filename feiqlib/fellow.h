#ifndef FELLOW_H
#define FELLOW_H

#include <string>
#include <memory>
#include <sstream>
#include <mutex>
using namespace std;

class Fellow
{
public:
    string getIp() const{lock_guard<mutex> lock(mMutex); return mIp;}
    string getName() const{lock_guard<mutex> lock(mMutex); return mName.empty() ? mPcName : mName;}
    string getHost() const{lock_guard<mutex> lock(mMutex); return mHost;}
    string getMac() const{lock_guard<mutex> lock(mMutex); return mMac;}
    bool isOnLine() const{lock_guard<mutex> lock(mMutex); return mOnLine;}
    string version() const{lock_guard<mutex> lock(mMutex); return mVersion;}

    void setIp(const string& value){
        lock_guard<mutex> lock(mMutex);
        mIp = value;
    }

    void setName(const string& value){
        lock_guard<mutex> lock(mMutex);
        mName = value;
    }

    void setHost(const string& value){
        lock_guard<mutex> lock(mMutex);
        mHost = value;
    }

    void setMac(const string& value){
        lock_guard<mutex> lock(mMutex);
        mMac = value;
    }

    void setOnLine(bool value){
        lock_guard<mutex> lock(mMutex);
        mOnLine = value;
    }

    void setVersion(const string& value){
        lock_guard<mutex> lock(mMutex);
        mVersion = value;
    }

    void setPcName(const string& value){
        lock_guard<mutex> lock(mMutex);
        mPcName = value;
    }

    bool update(const Fellow& fellow)
    {
        if (this == &fellow)
            return false;

        //按地址顺序上锁，避免不同 Fellow 之间互锁时死锁
        const mutex* first = this < &fellow ? &mMutex : &fellow.mMutex;
        const mutex* second = this < &fellow ? &fellow.mMutex : &mMutex;
        unique_lock<mutex> lock1(*const_cast<mutex*>(first), defer_lock);
        unique_lock<mutex> lock2(*const_cast<mutex*>(second), defer_lock);
        lock(lock1, lock2);

        bool changed = false;

        if (!fellow.mName.empty() && mName != fellow.mName){
            mName = fellow.mName;
            changed=true;
        }

        if (!fellow.mMac.empty() && mMac != fellow.mMac){
            mMac = fellow.mMac;
            changed=true;
        }

        if (mOnLine != fellow.mOnLine){
            mOnLine = fellow.mOnLine;
            changed=true;
        }

        return changed;
    }

    bool operator == (const Fellow& fellow)
    {
        return isSame(fellow);
    }

    bool isSame(const Fellow& fellow)
    {
        if (this == &fellow)
            return true;

        const mutex* first = this < &fellow ? &mMutex : &fellow.mMutex;
        const mutex* second = this < &fellow ? &fellow.mMutex : &mMutex;
        unique_lock<mutex> lock1(*const_cast<mutex*>(first), defer_lock);
        unique_lock<mutex> lock2(*const_cast<mutex*>(second), defer_lock);
        lock(lock1, lock2);

        return mIp == fellow.mIp || (!mMac.empty() && mMac == fellow.mMac);
    }

    string toString() const
    {
        lock_guard<mutex> lock(mMutex);
        ostringstream os;
        os<<"["
         <<"ip="<<mIp
        <<",name="<<mName
        <<",host="<<mHost
        <<",pcname="<<mPcName
        <<",mac="<<mMac
        <<",online="<<mOnLine
        <<",version="<<mVersion
        <<"]";
        return os.str();
    }

private:
    string mIp;
    string mPcName;
    string mName;
    string mHost;
    string mMac;
    bool mOnLine = false;
    string mVersion;
    mutable mutex mMutex;
};

#endif // FELLOW_H
