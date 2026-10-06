<div align=center>
<img src="icon.png" style="width:100px;" width="100"/>
<h2>我Q WorkQ</h2>
</div>

[English](README.en.md) | 简体中文

### 一、产品概述

- 基于 Qt 的跨平台即时通讯客户端，支持 Windows、macOS、Linux，覆盖 x86_64 / arm64 架构。
- 中文名为“我Q”，英文名为 WorkQ；通信协议与飞秋 / IPMSG 兼容，可直接与局域网中的飞秋用户互发文本、文件与表情。
- 支持局域网广播模式与 WorkQ-Server 远程服务器模式，兼顾本地聊天与跨网段协作。
- 收发文本与文件沿用原协议的 GBK 编码和字节流格式，保持经典飞秋的使用习惯。
- **发行方** 由异猫工作群（mutantcat.org）发行，GitHub: https://github.com/Mutantcat-Working-Group

核心价值：

- 开箱即用：无需中心服务器，同一局域网内自动发现好友，可直接与飞秋互操作。
- 跨平台：一套代码覆盖 Windows、macOS、Linux，并支持 x86_64 / arm64 构建。
- 远程协作：接入 WorkQ-Server 后支持账号、私聊/群聊、历史消息与在线状态，不受广播网段限制。
- 现代化界面：采用 QSS 统一视觉风格，保留经典飞秋操作习惯。
- 图标统一：所有平台安装包均使用仓库根目录的 `icon.png` 作为应用图标。

### 二、功能说明

#### 消息与文件

- 收发文本、文件，与飞秋互发表情
- 窗口抖动提醒，未读消息好友自动置顶
- 系统托盘气泡通知，点击可跳转到对应会话
- 定时更新好友列表，可选按沟通频繁度排序

#### 好友与网络

- 自动发现局域网好友，支持指定 IP 添加好友
- 自定义网段穿透屏蔽了广播包的路由器
- 查找好友，支持按名称搜索

#### 远程服务器模式

- 账号登录（WorkQ-Server）、记住密码、测试连接
- 私聊 / 群聊频道、历史消息分页、已读状态
- 用户搜索、在线状态、正在输入提示
- 断线自动重连，频道列表随服务器状态同步

### 三、安装与下载

从 [Releases](https://github.com/Mutantcat-Working-Group/WorkQ/releases/latest) 下载对应平台安装包：

| 平台 | 产物 |
| --- | --- |
| Linux x86_64 | `WorkQ-<版本>-linux-x86_64.AppImage` |
| macOS x86_64 | `WorkQ-<版本>-macos-x86_64.dmg` |
| macOS arm64 | `WorkQ-<版本>-macos-arm64.dmg` |
| Windows x86_64 | `WorkQ-<版本>-windows-x86_64-setup.exe` |
| Windows arm64 | `WorkQ-<版本>-windows-arm64-setup.exe` |

版本号按 `1.0.<构建日期>` 组织，例如 `1.0.20260930`。

1. **Linux**：下载 AppImage 后赋予执行权限，直接运行。
   ```bash
   chmod +x WorkQ-*.AppImage
   ./WorkQ-*.AppImage
   ```
2. **macOS**：打开 DMG，将“我Q”拖入“Applications”文件夹即可。
3. **Windows**：运行安装包，按提示完成安装，安装包为简体中文界面。

### 四、快速上手

1. **设置身份**：启动后在设置中填写用户名与主机名，保存后自动写入 `~/.workq_setting.ini`。
2. **添加好友**：局域网好友会自动发现；对方不在广播范围内时，通过“指定 IP 添加好友”手动加入。
3. **发送消息**：双击好友开始聊天，`Ctrl+Enter`（macOS 为 `Cmd+Enter`）发送，`Enter` 换行。
4. **发送文件**：将文件拖入聊天窗口，或点击工具栏的文件按钮选择文件。
5. **远程模式**：需要跨网段聊天时，先部署 [WorkQ-Server](https://github.com/Mutantcat-Working-Group/WorkQ-Server)，然后在“服务器设置”中启用远程服务器模式，填写地址、用户名与密码并测试连接。

### 五、配置

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

[server]
enabled = 1 ;启用远程服务器模式
url = http://host:port
username = user
remember = 1 ;记住密码
```

### 六、平台差异

* 通知：各平台均使用系统托盘气泡（`QSystemTrayIcon`），点击气泡或托盘图标可跳到对应会话。
* 未读角标：暂未实现原生角标，未读数显示在好友列表条目中。
* 发送快捷键：`Ctrl+Enter` 与 mac 的 `Cmd+Enter` 均可发送。
* 图标：所有平台统一从根目录 `icon.png` 生成应用图标（macOS `.icns`、Windows `.ico`、Linux PNG）。

### 七、开发进度

- [X] 局域网文本、文件收发
- [X] 与飞秋互发表情
- [X] 查找好友、指定 IP 添加好友
- [X] 窗口抖动
- [X] 未读消息置顶与托盘通知
- [X] 定时更新好友列表
- [X] 现代化 QSS 界面
- [X] WorkQ-Server 远程服务器模式
- [X] GitHub Actions 跨平台打包发布
- [ ] 原生未读角标
- [ ] 图片、文件夹收发
- [ ] 日志完善

### 八、开发者

界面的实现与飞秋协议部分是分离的。

`feiqlib` 是通信、协议解析、MVC 构架部分，基于 C++11 封装，仅依赖 Qt 网络模块，可移植到 Windows/macOS/Linux。

界面部分基于 Qt Widgets 实现。平台相关特性集中在 `platformdepend.cpp`：
1. 系统托盘通知消息
2. 点击通知跳转到对应会话

引用代码，请注明代码出处。

[GPL-3.0](LICENSE)
