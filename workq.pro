#-------------------------------------------------
#
# Project created by QtCreator 2016-07-30T10:47:11
#
#-------------------------------------------------

QT       += core gui network widgets

CONFIG   += c++17

TARGET = WorkQ
TEMPLATE = app

lessThan(QT_MAJOR_VERSION, 6): error("WorkQ 需要 Qt 6 或更高版本")

# 统一图标源为根目录 icon.png；CI 会由它生成 res/workq.ico 和 icon.icns
win32: RC_ICONS = res/workq.ico
macx: ICON = icon.icns

SOURCES += main.cpp\
        mainwindow.cpp \
    feiqlib/udpcommu.cpp \
    feiqlib/feiqcommu.cpp \
    feiqlib/feiqengine.cpp \
    feiqlib/feiqmodel.cpp \
    feiqlib/encoding.cpp \
    feiqlib/tcpserver.cpp \
    feiqlib/tcpsocket.cpp \
    feiqlib/utils.cpp \
    feiqlib/uniqueid.cpp \
    feiqlib/filetask.cpp \
    feiqlib/defer.cpp \
    feiqlib/asynwait.cpp \
    fellowlistwidget.cpp \
    searchfellowdlg.cpp \
    recvtextedit.cpp \
    filemanagerdlg.cpp \
    addfellowdialog.cpp \
    emoji.cpp \
    chooseemojidlg.cpp \
    platformdepend.cpp \
    chooseemojiwidget.cpp \
    sendtextedit.cpp \
    feiqwin.cpp \
    plugin/iplugin.cpp \
    plugin/rankuser.cpp \
    settings.cpp


HEADERS  += mainwindow.h \
    feiqlib/ipmsg.h \
    feiqlib/udpcommu.h \
    feiqlib/feiqcommu.h \
    feiqlib/protocol.h \
    feiqlib/post.h \
    feiqlib/content.h \
    feiqlib/feiqengine.h \
    feiqlib/feiqmodel.h \
    feiqlib/fellow.h \
    feiqlib/ifeiqview.h \
    feiqlib/msgqueuethread.h \
    feiqlib/encoding.h \
    feiqlib/tcpserver.h \
    feiqlib/tcpsocket.h \
    feiqlib/utils.h \
    feiqlib/uniqueid.h \
    feiqlib/filetask.h \
    feiqlib/defer.h \
    feiqlib/asynwait.h \
    feiqlib/parcelable.h \
    fellowlistwidget.h \
    searchfellowdlg.h \
    recvtextedit.h \
    filemanagerdlg.h \
    addfellowdialog.h \
    emoji.h \
    chooseemojidlg.h \
    platformdepend.h \
    chooseemojiwidget.h \
    sendtextedit.h \
    plugin/iplugin.h \
    feiqwin.h \
    plugin/rankuser.h \
    settings.h

FORMS    += mainwindow.ui \
    searchfellowdlg.ui \
    downloadfiledlg.ui \
    addfellowdialog.ui \
    chooseemojidlg.ui

RESOURCES += \
    default.qrc
