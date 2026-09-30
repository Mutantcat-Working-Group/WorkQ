#ifndef IPLUGIN_H
#define IPLUGIN_H

#include "../feiqwin.h"
#include <unordered_map>
#include <iostream>

class IPlugin
{
public:
    virtual ~IPlugin();
    void setFeiqWin(FeiqWin* feiqWin);
    virtual void init();
    virtual void unInit();

protected:
    FeiqWin* mFeiq = nullptr;
};

class PluginManager
{
private:
    PluginManager();
public:
    static PluginManager& instance();
public:
    std::unordered_map<const char*, IPlugin *> allPlugins;
};

#define REGISTER_PLUGIN(name, PluginCls)\
static bool register_##PluginCls = [](){\
    PluginManager::instance().allPlugins[name]=new PluginCls();\
    return true;\
}();
#endif // IPLUGIN_H
