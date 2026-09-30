#include "fellowlistwidget.h"
#include <QDebug>

FellowListWidget::FellowListWidget()
{

}

void FellowListWidget::bindTo(QListWidget *widget)
{
    mWidget = widget;
    connect(mWidget, &QListWidget::itemClicked, this, &FellowListWidget::itemChosen);
}

void FellowListWidget::update(const Fellow &fellow)
{
    auto item = findFirstItem(fellow);
    if (item == nullptr)
    {
        int row = requestRow(fellow);
        mWidget->insertItem(row, fellowText(fellow));
        item = mWidget->item(row);
        if (item == nullptr)
            return;
    }
    else
    {
        item->setText(fellowText(fellow));
    }

    item->setData(Qt::UserRole, QVariant::fromValue((void*)&fellow));
}

void FellowListWidget::top(const Fellow &fellow)
{
    auto item = findFirstItem(fellow);
    if (item != nullptr)
    {
        mWidget->takeItem(mWidget->row(item));
        mWidget->insertItem(0, item);
        mWidget->scrollToItem(item);
        mWidget->setCurrentItem(item);
    }
}

//TODO:take->insert的方式或导致如果item是当前焦点，则移动后焦点丢失
void FellowListWidget::topSecond(const Fellow &fellow)
{
    auto item = findFirstItem(fellow);
    if (item != nullptr)
    {
        mWidget->takeItem(mWidget->row(item));
        mWidget->insertItem(1, item);
    }
}

void FellowListWidget::mark(const Fellow &fellow, const QString &info)
{
    auto item = findFirstItem(fellow);
    if (item != nullptr)
    {
        if (info.isEmpty())
            item->setText(fellowText(fellow));
        else
        {
            item->setText("("+info+")"+fellowText(fellow));
            if (mWidget->row(item)!=0)
            {
                if (mWidget->currentRow() == 0)
                    topSecond(fellow);
                else
                    top(fellow);
            }
        }
    }
}

void FellowListWidget::remove(const Fellow &fellow)
{
    auto item = findFirstItem(fellow);
    if (item != nullptr)
        delete mWidget->takeItem(mWidget->row(item));
}

void FellowListWidget::removeRemote()
{
    for (int i = mWidget->count() - 1; i >= 0; --i)
    {
        auto item = mWidget->item(i);
        auto fellow = getFellow(item);
        if (fellow && fellow->isRemote())
            delete mWidget->takeItem(i);
    }
}

void FellowListWidget::setRankPredict(FellowListWidget::RankPredict predict)
{
    mRankPredict = predict;
}

void FellowListWidget::itemChosen(QListWidgetItem *item)
{
    if (item == nullptr)
        return;

    auto fellow = getFellow(item);
    emit select(fellow);
}

QString FellowListWidget::fellowText(const Fellow &fellow)
{
    QString text;
    const int unread = fellow.getUnreadCount();
    if (unread > 0)
        text += QString("[%1]").arg(unread);
    text += QString::fromStdString(fellow.getName());
    text += ",";
    text += QString::fromStdString(fellow.getIp());
    if (!fellow.isOnLine())
        text = "[离线]"+text;
    const QString typing = QString::fromStdString(fellow.getTyping());
    if (!typing.isEmpty())
        text += "（"+typing+" 正在输入...）";
    return text;
}

QListWidgetItem *FellowListWidget::findFirstItem(const Fellow &fellow)
{
    auto count = mWidget->count();
    for (int i = 0; i < count; i++)
    {
        auto widget = mWidget->item(i);
        auto f = getFellow(widget);
        if (f != nullptr && f->getIp() == fellow.getIp())
            return widget;
    }

    return nullptr;
}

int FellowListWidget::requestRow(const Fellow &fellow)
{
    auto count = mWidget->count();

    if (!mRankPredict)
        return count;

    int row = count;
    for (; row > 0; row--)
    {
        auto f = getFellow(mWidget->item(row-1));
        if (f == nullptr)
            continue;
        auto ret = mRankPredict(*f, fellow);
        if (ret >= 0)
            break;
    }

    return row;
}

const Fellow *FellowListWidget::getFellow(const QListWidgetItem *item)
{
    if (item == nullptr)
        return nullptr;
    return static_cast<const Fellow*>(item->data(Qt::UserRole).value<void*>());
}
