# 我Q（WorkQ）

基于 Qt 的跨平台飞鸽传书（IPMSG）客户端，支持 Windows、macOS、Linux。

软件中文名为“我Q”，英文名为 WorkQ。通信协议保持与飞秋/IPMSG 兼容，收发文本与文件均沿用原协议的 GBK 编码和字节流格式。

## 支持特性
* 收发文本、文件
* 可与飞秋互发表情
* 查找好友
* 窗口抖动
* 指定 IP 添加好友
* 自定义网段穿透屏蔽了广播包的路由器
* 未读消息的好友自动置顶
* 定时更新好友列表
* 未读消息通知（通过系统托盘气泡）
* 按沟通频繁度排序好友（可选）

## 构建

项目使用 qmake 构建，需要 Qt 6（C++17）开发环境。`workq.pro` 会在 Qt 6 以下版本直接报错：

```bash
qmake workq.pro
make
```

Windows 下可在 Qt Creator 中直接打开 `workq.pro`，或使用 MinGW/MSVC 工具链编译。
macOS/Linux 下安装对应平台的 Qt 6 开发包后执行上面的命令即可。

## 自动打包

仓库内置 GitHub Actions（`.github/workflows/build.yml`），推送、PR 或手动触发后会自动产出 Linux AppImage、macOS 双架构 DMG、Windows x86_64/arm64 NSIS 安装包。详见 [docs/ci.md](docs/ci.md)。

## 配置

配置文件为 `~/.workq_setting.ini`。首次启动时若检测到旧版 `~/.feiq_setting.ini`，会自动复制并迁移，旧文件不会被删除。

```ini
[user]
name = 我Q用户  ;设置用户名
host = WorkQ   ;设置主机名

[app]
title = 我Q    ;设置窗口标题
send_by_enter = 0 ;0：ctrl/cmd+enter发送，enter回车；1：相反

[network]
custom_group = 192.168.74.|192.168.82. ;设置一些广播包无法触及的子网，点号结束一个网段的定义，竖线分隔各个网段

[rank_user]
enable = 1 ;启用按沟通频繁度排序用户的功能
```

## 平台差异

* 通知：各平台均使用系统托盘气泡（`QSystemTrayIcon`），点击气泡或托盘图标可跳到对应会话。
* 未读角标：暂未实现原生角标，未读数显示在好友列表条目中。
* 发送快捷键：`Ctrl+Enter` 与 mac 的 `Cmd+Enter` 均可发送。

## 尚未支持的特性
* 设置、显示文本格式
* 图片收发：仅支持获取图片 id，图片数据的协议未破解
* 文件夹收发：飞秋使用了自定义的文件夹收发协议
* 日志：部分完成

## 开发者

界面的实现与飞秋协议部分是分离的。

`feiqlib` 是通信、协议解析、MVC 构架部分，基于 C++11 封装，仅依赖 Qt 网络模块，可移植到 Windows/macOS/Linux。

界面部分基于 Qt Widgets 实现。平台相关特性集中在 `platformdepend.cpp`：
1. 系统托盘通知消息
2. 点击通知跳转到对应会话

引用代码，请注明代码出处。
