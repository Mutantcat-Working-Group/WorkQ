#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include "feiqwin.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("WorkQ");
    a.setApplicationDisplayName("我Q");
    a.setWindowIcon(QIcon(":/default/res/icon.png"));

    QFile styleFile(":/default/ui.qss");
    if (styleFile.open(QIODevice::ReadOnly))
        a.setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    MainWindow w;
    FeiqWin feiqWin;
    w.setFeiqWin(&feiqWin);

    w.show();
    return a.exec();
}
