<div align=center>
<img src="icon.png" style="width:100px;" width="100"/>
<h2>我Q WorkQ</h2>
</div>

[中文](README.md) | English

### 1. Overview

- A cross-platform instant messaging client built on Qt, supporting Windows, macOS, and Linux, with x86_64 / arm64 builds.
- The Chinese product name is "我Q" and the English name is WorkQ. The protocol stays compatible with Feiq / IPMSG, so it can exchange text, files, and emoji with Feiq users on the same LAN.
- Supports both the classic LAN broadcast mode and the WorkQ-Server remote server mode, covering local chat and cross-network collaboration.
- Text and file transfer follows the original GBK encoding and byte-stream format, preserving the classic Feiq workflow.
- **Publisher** Released by Mutantcat Working Group (mutantcat.org), GitHub: https://github.com/Mutantcat-Working-Group

Core values:

- Ready to use out of the box: no central server is required, friends on the same LAN are discovered automatically, and interoperability with Feiq works out of the box.
- Cross-platform: one codebase covers Windows, macOS, and Linux, with x86_64 / arm64 builds.
- Remote collaboration: connecting to WorkQ-Server adds accounts, private/group channels, message history, and presence without being limited by broadcast subnets.
- Modern interface: a unified QSS visual style while keeping the classic Feiq interaction patterns.
- Unified icon: all platform packages use `icon.png` from the repository root as the application icon.

### 2. Features

#### Messages and Files

- Send and receive text and files, and exchange emoji with Feiq
- Window knock reminders, friends with unread messages are pinned to the top
- System tray bubble notifications, click to jump to the matching conversation
- Scheduled friend list refresh, with optional sorting by chat frequency

#### Friends and Network

- Automatically discover LAN friends, with support for adding friends by IP address
- Custom subnet groups that penetrate routers blocking broadcast packets
- Friend search by name

#### Remote Server Mode

- Account login (WorkQ-Server), remember password, and connection testing
- Private / group channels, paginated message history, and read state
- User search, presence, and typing indicators
- Automatic reconnection and channel list synchronization with the server state

### 3. Installation and Download

Download the installer for your platform from the [Releases](https://github.com/Mutantcat-Working-Group/WorkQ/releases/latest) page:

| Platform | Artifact |
| --- | --- |
| Linux x86_64 | `WorkQ-<version>-linux-x86_64.AppImage` |
| macOS x86_64 | `WorkQ-<version>-macos-x86_64.dmg` |
| macOS arm64 | `WorkQ-<version>-macos-arm64.dmg` |
| Windows x86_64 | `WorkQ-<version>-windows-x86_64-setup.exe` |
| Windows arm64 | `WorkQ-<version>-windows-arm64-setup.exe` |

Versions follow the `1.0.<build date>` format, for example `1.0.20260930`.

1. **Linux**: Download the AppImage, make it executable, and run it directly.
   ```bash
   chmod +x WorkQ-*.AppImage
   ./WorkQ-*.AppImage
   ```
2. **macOS**: Open the DMG and drag "我Q" to the "Applications" folder.
3. **Windows**: Run the installer and follow the prompts. The installer uses a simplified Chinese UI.

### 4. Quick Start

1. **Set your identity**: After starting, set the username and host name in the settings; they are saved to `~/.workq_setting.ini`.
2. **Add friends**: LAN friends are discovered automatically. If a friend is outside the broadcast range, use "Add friend by IP".
3. **Send messages**: Double-click a friend to start chatting. Press `Ctrl+Enter` (`Cmd+Enter` on macOS) to send, `Enter` for a new line.
4. **Send files**: Drag a file into the chat window, or click the file button in the toolbar to select a file.
5. **Remote mode**: For cross-network chat, deploy [WorkQ-Server](https://github.com/Mutantcat-Working-Group/WorkQ-Server) first, then enable remote server mode in "Server settings", fill in the address, username, and password, and test the connection.

### 5. Configuration

The configuration file is `~/.workq_setting.ini`. On first launch, an old `~/.feiq_setting.ini` is copied and migrated automatically; the old file is not deleted.

```ini
[user]
name = WorkQ user  ;user name
host = WorkQ       ;host name

[app]
title = 我Q        ;window title
send_by_enter = 0 ;0: ctrl/cmd+enter sends, enter is newline; 1: inverted

[network]
custom_group = 192.168.74.|192.168.82. ;subnets unreachable by broadcast, ending each subnet with a dot and separating subnets with a vertical bar

[rank_user]
enable = 1 ;sort friends by chat frequency

[server]
enabled = 1 ;enable remote server mode
url = http://host:port
username = user
remember = 1 ;remember password
```

### 6. Platform Notes

* Notifications use the system tray bubble (`QSystemTrayIcon`) on all platforms; clicking a bubble or the tray icon jumps to the matching conversation.
* Native unread badges are not implemented yet; unread counts are shown in the friend list entries.
* Send shortcuts: `Ctrl+Enter` and `Cmd+Enter` on macOS both send.
* Icons: all platforms generate the application icon from `icon.png` at the repository root (`.icns` on macOS, `.ico` on Windows, PNG on Linux).

### 7. Development Progress

- [X] LAN text and file transfer
- [X] Emoji exchange with Feiq
- [X] Friend search and add friend by IP
- [X] Window knock
- [X] Unread pinning and tray notifications
- [X] Scheduled friend list refresh
- [X] Modern QSS interface
- [X] WorkQ-Server remote server mode
- [X] GitHub Actions cross-platform packaging and release
- [ ] Native unread badges
- [ ] Image and folder transfer
- [ ] Complete logging

### 8. For Developers

The UI and the Feiq protocol implementation are separated.

`feiqlib` handles communication, protocol parsing, and the MVC architecture. It is based on C++11 and only depends on the Qt network module, so it is portable across Windows, macOS, and Linux.

The UI is built with Qt Widgets. Platform-specific features are concentrated in `platformdepend.cpp`:
1. System tray notification messages
2. Jumping to the matching conversation when a notification is clicked

Please credit the source when reusing the code.

[GPL-3.0](LICENSE)
