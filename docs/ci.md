# GitHub Actions 打包

仓库的 `.github/workflows/build.yml` 会为每个 `push`（含 `v*` 标签）、`pull_request` 和手动触发构建一次产物：

| 平台 | 产物 |
| --- | --- |
| Linux x86_64 | `WorkQ-<版本>-linux-x86_64.AppImage` |
| macOS x86_64 | `WorkQ-<版本>-macos-x86_64.dmg` |
| macOS arm64 | `WorkQ-<版本>-macos-arm64.dmg` |
| Windows x86_64 | `WorkQ-<版本>-windows-x86_64-setup.exe` |
| Windows arm64 | `WorkQ-<版本>-windows-arm64-setup.exe` |

## 打包细节

- macOS 使用 Qt 官方 `clang_64` 包，分别在 Intel 和 Apple Silicon runner 上构建；`macdeployqt` 收集依赖后对整个 `.app` 做 ad-hoc 签名（`codesign --force --deep --sign -`）。
- macOS DMG 由 `hdiutil` 生成，DMG 根目录内包含指向 `/Applications` 的 `Applications` 快捷方式，并放置应用图标。
- Windows 使用 NSIS 3 Unicode 版，界面为简体中文，左下角品牌文本显示为“我Q WorkQ v<版本>”，不再显示 Nullsoft Install System 文案。
- Windows 的安装包和 `WorkQ.exe` 使用 CI 内临时生成的自签名代码签名证书签名，仅用于消除无签名提示；正式发布如需公开信任，可改为配置真实的 PFX/证书 secret。
- Linux AppImage 通过 `linuxdeploy` + Qt 插件生成，附带桌面文件和图标。
- Windows arm64 使用 `win64_msvc2022_arm64` 的 Qt 包，在 x64 runner 上通过 MSVC 的 `amd64_arm64` 交叉编译，其余 Windows 步骤与 x86_64 相同。

版本号固定为 `1.0.<构建日期>`，构建日期按北京时间（UTC+8）当天计算，例如 2026-09-30 构建得到 `1.0.20260930`。需要调整主/次版本时改 workflow 里的 `1.0.` 前缀即可。

## 本地手动打包

Linux：

```bash
qmake workq.pro CONFIG+=release
make -j$(nproc)
```

macOS：

```bash
qmake workq.pro CONFIG+=release
make -j$(sysctl -n hw.ncpu)
macdeployqt WorkQ.app
codesign --force --deep --sign - WorkQ.app
hdiutil create -volname WorkQ -srcfolder WorkQ.app -ov -format UDZO WorkQ.dmg
```

注意：上面的 `hdiutil` 命令只是最小示例；仓库 CI 里的 DMG 会先放入一个包含 `Applications` 快捷方式（指向 `/Applications` 的软链接）的目录再生成。

Windows（MSVC 环境）：

```powershell
qmake workq.pro CONFIG+=release
nmake
windeployqt --release --compiler-runtime release\WorkQ.exe
$root = (Get-Location).Path
makensis /DVERSION=1.0.0 /DARCH=x86_64 "/DSOURCE_DIR=$root\release" "/DOUTPUT_DIR=$root" "${root}\packaging\windows\installer.nsi"
```

NSIS 脚本里的相对路径以脚本所在目录为基准，本地编译时请像上面一样传入绝对路径，安装包会输出到 `$root`。
