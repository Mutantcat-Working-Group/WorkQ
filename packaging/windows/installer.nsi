; WorkQ / 我Q Windows NSIS installer.
; Run from the repository root with:
;   makensis /DVERSION=1.0.0 /DARCH=x86_64 /DSOURCE_DIR=C:\path\to\release installer.nsi

Unicode true

!include "MUI2.nsh"

!ifndef VERSION
  !define VERSION "1.0.0"
!endif
!ifndef ARCH
  !define ARCH "x86_64"
!endif
!ifndef SOURCE_DIR
  !define SOURCE_DIR "${__FILEDIR__}\..\..\release"
!endif
!ifndef ICON_FILE
  !define ICON_FILE "${__FILEDIR__}\..\..\res\workq.ico"
!endif
!ifndef OUTPUT_DIR
  !define OUTPUT_DIR "${__FILEDIR__}"
!endif

!define APP_NAME "我Q"
!define APP_EN_NAME "WorkQ"
!define APP_DISPLAY "${APP_NAME} ${APP_EN_NAME}"

Name "${APP_DISPLAY}"
Caption "${APP_DISPLAY} ${VERSION} 安装程序"
BrandingText "${APP_NAME} ${APP_EN_NAME} v${VERSION}"
OutFile "${OUTPUT_DIR}\WorkQ-${VERSION}-windows-${ARCH}-setup.exe"
InstallDir "$PROGRAMFILES64\WorkQ"
InstallDirRegKey HKCU "Software\WorkQ" ""
RequestExecutionLevel admin
SetCompressor /SOLID lzma
SetCompressorDictSize 64

!define MUI_ABORTWARNING
!define MUI_ICON "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN "$INSTDIR\WorkQ.exe"
!define MUI_FINISHPAGE_RUN_TEXT "立即运行 ${APP_DISPLAY}"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APP_DISPLAY}"
VIAddVersionKey "FileDescription" "${APP_DISPLAY} 安装程序"
VIAddVersionKey "CompanyName" "WorkQ"
VIAddVersionKey "LegalCopyright" "Copyright (C) WorkQ"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"

Section "WorkQ" SEC_APP
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "${SOURCE_DIR}\*.*"

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WorkQ" "DisplayName" "${APP_DISPLAY}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WorkQ" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WorkQ" "Publisher" "WorkQ"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WorkQ" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WorkQ" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WorkQ" "DisplayIcon" "$INSTDIR\WorkQ.exe,0"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WorkQ" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WorkQ" "NoRepair" 1
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir /r "$INSTDIR"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WorkQ"
    DeleteRegKey HKCU "Software\WorkQ"
SectionEnd
