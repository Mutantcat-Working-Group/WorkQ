#ifndef SERVERSETTINGSDIALOG_H
#define SERVERSETTINGSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QPointer>

class Settings;
class ServerEngine;

class ServerSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ServerSettingsDialog(Settings *settings, ServerEngine *server,
                                  QWidget *parent = nullptr);

private slots:
    void saveAndClose();
    void testConnection();

private:
    Settings *mSettings;
    ServerEngine *mServer;
    QCheckBox *mEnableBox;
    QLineEdit *mUrlEdit;
    QLineEdit *mUserEdit;
    QLineEdit *mPassEdit;
    QCheckBox *mRememberBox;
    QLabel *mStatusLabel;
    QPointer<ServerEngine> mProbe;
};

#endif // SERVERSETTINGSDIALOG_H
