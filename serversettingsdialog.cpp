#include "serversettingsdialog.h"

#include "settings.h"
#include "serverengine.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

ServerSettingsDialog::ServerSettingsDialog(Settings *settings, ServerEngine *server,
                                           QWidget *parent)
    : QDialog(parent)
    , mSettings(settings)
    , mServer(server)
{
    setWindowTitle(QStringLiteral("服务器设置"));

    mEnableBox = new QCheckBox(QStringLiteral("启用远程服务器模式"), this);
    mEnableBox->setChecked(mSettings->value("server/enabled", false).toBool());

    mUrlEdit = new QLineEdit(mSettings->value("server/url").toString(), this);
    mUrlEdit->setPlaceholderText(QStringLiteral("http://host:port"));

    mUserEdit = new QLineEdit(mSettings->value("server/username").toString(), this);
    mPassEdit = new QLineEdit(mSettings->value("server/password").toString(), this);
    mPassEdit->setEchoMode(QLineEdit::Password);

    mRememberBox = new QCheckBox(QStringLiteral("记住密码"), this);
    mRememberBox->setChecked(mSettings->value("server/remember", false).toBool());

    mStatusLabel = new QLabel(this);

    auto *testBtn = new QPushButton(QStringLiteral("测试连接"), this);
    connect(testBtn, &QPushButton::clicked, this, &ServerSettingsDialog::testConnection);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存并重连"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ServerSettingsDialog::saveAndClose);
    connect(buttons, &QDialogButtonBox::rejected, this, &ServerSettingsDialog::reject);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("服务器地址"), mUrlEdit);
    form->addRow(QStringLiteral("用户名"), mUserEdit);
    form->addRow(QStringLiteral("密码"), mPassEdit);
    form->addRow(QString(), mRememberBox);
    form->addRow(QString(), mStatusLabel);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(mEnableBox);
    layout->addLayout(form);
    layout->addWidget(testBtn);
    layout->addWidget(buttons);
}

void ServerSettingsDialog::saveAndClose()
{
    const bool enabled = mEnableBox->isChecked();
    mSettings->setValue("server/enabled", enabled);
    mSettings->setValue("server/url", mUrlEdit->text().trimmed());
    mSettings->setValue("server/username", mUserEdit->text().trimmed());
    mSettings->setValue("server/remember", mRememberBox->isChecked());
    if (mRememberBox->isChecked())
        mSettings->setValue("server/password", mPassEdit->text());
    else
        mSettings->remove("server/password");
    mSettings->sync();
    accept();
}

void ServerSettingsDialog::testConnection()
{
    mStatusLabel->setText(QStringLiteral("正在连接..."));
    mStatusLabel->setStyleSheet(QStringLiteral("color: gray;"));

    const QString url = mUrlEdit->text().trimmed();
    const QString user = mUserEdit->text().trimmed();
    const QString pass = mPassEdit->text();
    if (url.isEmpty() || user.isEmpty())
    {
        mStatusLabel->setText(QStringLiteral("请先填写服务器地址和用户名"));
        mStatusLabel->setStyleSheet(QStringLiteral("color: red;"));
        return;
    }

    // 用临时引擎做一次登录，避免打断当前连接
    if (mProbe)
        mProbe->stop();
    mProbe = new ServerEngine(this);
    connect(mProbe.data(), &ServerEngine::stateChanged, this, [this](ServerEngine::State state, const QString &detail) {
        if (state == ServerEngine::State::Connected)
        {
            mStatusLabel->setText(QStringLiteral("连接成功"));
            mStatusLabel->setStyleSheet(QStringLiteral("color: green;"));
        }
        else if (state == ServerEngine::State::Disconnected)
        {
            mStatusLabel->setText(QStringLiteral("连接失败：%1").arg(detail));
            mStatusLabel->setStyleSheet(QStringLiteral("color: red;"));
        }
    });
    mProbe->start(url, user, pass);
}
