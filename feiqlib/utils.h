#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <charconv>
#include <system_error>
using namespace std;
vector<string> splitAllowSeperator(vector<char>::iterator from, vector<char>::iterator to, char sep);
void stringReplace(string& target,const string& pattern,const string& candidate);
string getFileNameFromPath(const string& path);
bool startsWith(const string& str, const string& patten);
bool endsWith(const string& str, const string& patten);
string toString(const vector<char>& buf);

//网络包解析不要依赖可能抛异常的 stoi/stoul 系列，统一使用安全解析
template<class T>
T safeParse(const string& str, T defaultValue, int base = 10)
{
    if (str.empty())
        return defaultValue;

    T value = defaultValue;
    auto result = from_chars(str.data(), str.data()+str.size(), value, base);
    if (result.ec == errc() && result.ptr == str.data()+str.size())
        return value;
    return defaultValue;
}
#endif // UTILS_H
