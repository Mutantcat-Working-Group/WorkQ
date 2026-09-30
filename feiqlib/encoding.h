#ifndef ENCODING_H
#define ENCODING_H

#include <vector>
#include <string>
#include <mutex>

#ifndef _WIN32
#include <iconv.h>
#endif

class Encoding
{
public:
    Encoding(const std::string& fromCharset, const std::string& toCharset);
    ~Encoding();
    std::vector<char> convert(const std::vector<char>& str);
    std::string convert(const std::string& str);

    /**
     * @brief convert 编码转换
     * @param input 源内存首地址
     * @param len 源长度
     * @param output 目标内存首地址
     * @param outLen 输入输出参数，输入目标缓冲区大小，输出实际使用大小
     * @return
     */
    bool convert(const char* input, size_t len, char* output, size_t* outLen);
private:
#ifndef _WIN32
    iconv_t mIconv;
    mutable std::mutex mIconvMutex;
#endif
    std::string mFromCharset;
    std::string mToCharset;
};

extern Encoding* encOut;
extern Encoding* encIn;


#endif // ENCODING_H
