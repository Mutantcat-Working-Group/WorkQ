#ifndef ADDFELLOWDIALOG_H
#define ADDFELLOWDIALOG_H

#include <QDialog>

namespace Ui {
class AddFellowDialog;
}

class AddFellowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddFellowDialog(QWidget *parent = 0);
    ~AddFellowDialog();

public:
    QString getIp();
    void setServerMode(bool serverMode);
private slots:
    void onOkClicked();
private:
    bool isValidIp(const QString& ip);

private:
    Ui::AddFellowDialog *ui;
    bool mServerMode = false;
};

#endif // ADDFELLOWDIALOG_H
