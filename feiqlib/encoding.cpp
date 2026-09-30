#include "encoding.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <iconv.h>
#endif

#ifdef _WIN32
static UINT encodingToCodePage(const std::string& charset)
{
    if (charset.find("UTF-8") != std::string::npos || charset.find("utf-8") != std::string::npos)
        return CP_UTF8;
    if (charset.find("GBK") != std::string::npos || charset.find("GB2312") != std::string::npos)
        return 936;
    return CP_ACP;
}
#endif

Encoding* encIn = new Encoding("GBK", "UTF-8");
Encoding* encOut = new Encoding("UTF-8", "GBK");

Encoding::Encoding(const std::string &fromCharset, const std::string &toCharset)
    : mFromCharset(fromCharset), mToCharset(toCharset)
{
#ifndef _WIN32
    //iconv_open is a freek(to goes first)
    mIconv = iconv_open(toCharset.c_str(), fromCharset.c_str());
#endif
}

Encoding::~Encoding()
{
#ifndef _WIN32
    if (mIconv != (iconv_t)-1)
        iconv_close(mIconv);
#endif
}

std::vector<char> Encoding::convert(const std::vector<char> &str)
{
    std::vector<char> result(str.size()*3);
    auto len = result.size();
    if (convert(str.data(), str.size(), result.data(), &len))
    {
        result.resize(len);
        result.shrink_to_fit();
        return result;
    }

    return str;
}

std::string Encoding::convert(const std::string &str)
{
    auto len = str.length()*3;
    std::unique_ptr<char[]> buf(new char[len]);
    if (convert(str.data(), str.size(), buf.get(), &len))
    {
        std::string result(buf.get(), len);
        return result;
    }

    return str;
}

bool Encoding::convert(const char *input, size_t len, char *output, size_t *outLen)
{
    if (input == nullptr || output == nullptr || outLen == nullptr)
        return false;

    if (len == 0)
    {
        *outLen = 0;
        return true;
    }

#ifdef _WIN32
    auto fromCodePage = encodingToCodePage(mFromCharset);
    auto toCodePage = encodingToCodePage(mToCharset);
    if (fromCodePage == toCodePage)
    {
        if (*outLen < len)
            return false;
        memcpy(output, input, len);
        *outLen = len;
        return true;
    }

    auto wideLen = MultiByteToWideChar(fromCodePage, 0, input, static_cast<int>(len), nullptr, 0);
    if (wideLen <= 0)
        return false;

    std::wstring wide(wideLen, L'\0');
    if (MultiByteToWideChar(fromCodePage, 0, input, static_cast<int>(len), &wide[0], wideLen) <= 0)
        return false;

    auto outLenNeeded = WideCharToMultiByte(toCodePage, 0, wide.c_str(), wideLen, nullptr, 0, nullptr, nullptr);
    if (outLenNeeded <= 0 || static_cast<size_t>(outLenNeeded) > *outLen)
        return false;

    if (WideCharToMultiByte(toCodePage, 0, wide.c_str(), wideLen, output, outLenNeeded, nullptr, nullptr) <= 0)
        return false;

    *outLen = static_cast<size_t>(outLenNeeded);
    return true;
#else
    if (mIconv == (iconv_t)-1)
        return false;

    //copy in str
    size_t inLen = len;
    auto inBuf = std::unique_ptr<char[]>(new char[inLen+1]);
    char* pIn = inBuf.get();
    memcpy(pIn, input, inLen);
    pIn[inLen]=0;

    //do convert
    {
        std::lock_guard<std::mutex> lock(mIconvMutex);
        size_t outCapacity = *outLen;
        int ret = iconv(mIconv, &pIn, &inLen, &output, outLen);
        if (ret == -1)
        {
            perror("convert failed");
            //重置内部状态，避免半途的多字节状态影响后续转换
            iconv(mIconv, nullptr, nullptr, nullptr, nullptr);
            return false;
        }
        *outLen = outCapacity - *outLen;
    }

    return true;
#endif
}
