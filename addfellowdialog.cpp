#include "addfellowdialog.h"
#include "ui_addfellowdialog.h"
#include <QHostAddress>
#include <QMessageBox>

AddFellowDialog::AddFellowDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddFellowDialog)
{
    ui->setupUi(this);
    connect(ui->okBtn, &QPushButton::clicked, this, &AddFellowDialog::onOkClicked);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &AddFellowDialog::reject);
}

AddFellowDialog::~AddFellowDialog()
{
    delete ui;
}

QString AddFellowDialog::getIp()
{
    return ui->ipEdit->text();
}

void AddFellowDialog::onOkClicked()
{
    auto ip = ui->ipEdit->text();
    if (mServerMode)
    {
        if (!ip.trimmed().isEmpty())
            accept();
        else
            QMessageBox::warning(this, "用户名无效", "要添加的用户名不能为空");
    }
    else if (isValidIp(ip))
    {
        accept();
    }
    else
    {
        QMessageBox::warning(this, "ip地址无效", "要添加的ip地址无效");
    }
}

void AddFellowDialog::setServerMode(bool serverMode)
{
    mServerMode = serverMode;
    if (serverMode)
    {
        ui->promptLabel->setText("输入要添加的用户名：");
        ui->ipEdit->setPlaceholderText("请输入服务器用户名");
    }
    else
    {
        ui->promptLabel->setText("输入要添加的好友 IP：");
        ui->ipEdit->setPlaceholderText("请输入要添加的好友ip");
    }
}

bool AddFellowDialog::isValidIp(const QString &ip)
{
    QHostAddress address;
    return address.setAddress(ip);
}
