/**
Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed by Gabriel Caudrelier, gabriel dot caudrelier at nccgroup dot com

https://github.com/nccgroup/pip3line

Released under AGPL see LICENSE for more information
**/

#include "quickviewdialog.h"
#include "ui_quickviewdialog.h"
#include <QDebug>

QuickViewDialog::QuickViewDialog(GuiHelper *nguiHelper, QWidget *parent) :
    QDialog(parent)
{
    ui = new(std::nothrow) Ui::QuickViewDialog();
    if (ui == NULL) {
        qFatal("Cannot allocate memory for Ui::QuickViewDialog X{");
    }
    guiHelper = nguiHelper;
    ui->setupUi(this);
    setModal(false);
    connect(ui->addPushButton, SIGNAL(clicked()), this, SLOT(newItem()));
    connect(ui->resetPushButton, SIGNAL(clicked()), this, SLOT(onReset()));

    QStringList saved = guiHelper->getQuickViewConf();
    for (int i = 0; i < saved.size(); i++) {
        addItem(saved.at(i));
    }
}

QuickViewDialog::~QuickViewDialog()
{
    QStringList saving;
    listLock.lockForRead();
    for (int i = 0; i < itemList.size(); i++) {
        saving.append(itemList.at(i)->getXmlConf());
        // don't need to delete them, the ui will do it
    }
    listLock.unlock();
    guiHelper->saveQuickViewConf(saving);

    delete ui;
}

void QuickViewDialog::newItem()
{
    QuickViewItem * qvi = new(std::nothrow) QuickViewItem(guiHelper, this);
    if (qvi != NULL) {
        listLock.lockForWrite();
        itemList.append(qvi);
        listLock.unlock();
        connect(qvi, SIGNAL(destroyed()), this, SLOT(itemDeleted()));
        if (qvi->configure()) {
            ui->itemLayout->addWidget(qvi);
            dataLock.lockForRead();
            qvi->processData(currentData);
            dataLock.unlock();
        }
    } else {
        qFatal("Cannot allocate memory for QuickViewItem for newItem X{");
    }
}

void QuickViewDialog::itemDeleted()
{
    QuickViewItem * item = (QuickViewItem *)sender();
    listLock.lockForWrite();
    int i = itemList.removeAll(item);
    listLock.unlock();
    if (i == 0) { // something is wrong
        guiHelper->getLogger()->logError(tr("QuickView item not found 0x%1").arg(QString::number((quintptr)item,16)),"QuickView");
    } else if (i > 1) { // something is really wrong
        guiHelper->getLogger()->logError(tr("Multiple QuickView item found 0x%1: %2").arg(QString::number((quintptr)item,16)).arg(i),"QuickView");
    }
}

void QuickViewDialog::onReset()
{
    listLock.lockForRead();
    QList<QuickViewItem *> list = itemList;
    listLock.unlock();
    for (int i = 0; i < list.size(); i++) {
        delete list.at(i);
    }

    guiHelper->saveQuickViewConf(QStringList());

    QStringList saved = guiHelper->getQuickViewConf();
    for (int i = 0; i < saved.size(); i++) {
        addItem(saved.at(i));
    }

}

void QuickViewDialog::addItem(const QString &conf)
{
    QuickViewItem * qvi = new(std::nothrow) QuickViewItem(guiHelper, this, conf);
    if (qvi != NULL) {
        if (!qvi->isConfigured()) {
            delete qvi;
            return;
        }
        listLock.lockForWrite();
        itemList.append(qvi);
        listLock.unlock();
        ui->itemLayout->addWidget(qvi);
        dataLock.lockForRead();
        qvi->processData(currentData);
        dataLock.unlock();
        connect(qvi, SIGNAL(destroyed()), this, SLOT(itemDeleted()));
    } else {
        qFatal("Cannot allocate memory for QuickViewItem for addItem X{");
    }
}

void QuickViewDialog::receivingData(const QByteArray &data)
{
    dataLock.lockForWrite();
    currentData = data;
    dataLock.unlock();

    listLock.lockForRead();
    for (int i = 0; i < itemList.size(); i++) {
        itemList.at(i)->processData(currentData);
    }
    listLock.unlock();

}


