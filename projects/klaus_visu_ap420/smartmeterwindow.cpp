#include <stdio.h>
#include "smartmeterwindow.h"
#include "ui_smartmeterwindow.h"

smartmeterwindow::smartmeterwindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::smartmeterwindow) {
    ui->setupUi(this);
    isVisible = false;

    connect(this, SIGNAL(serviceCmd(const moduleservice::cmd *, QDialog *)),
            parent, SLOT(onSendServiceCmd(const struct moduleservice::cmd *, QDialog *)));
    connect(parent, SIGNAL(cmdConf(const struct moduleservice::result *, QDialog *)),
            this, SLOT(onCmdConf(const struct moduleservice::result *, QDialog *)));
    connect(parent, SIGNAL(screenSaverActivated()), this, SLOT(onScreenSaverActivation()));

    updTimer = new QTimer(this);
    connect(updTimer, SIGNAL(timeout()), this, SLOT(updTimerEvent()));
    updTimer->start(1100);
}

smartmeterwindow::~smartmeterwindow() {
    delete ui;
}

void smartmeterwindow::onScreenSaverActivation(void) {
    if (isVisible) {
        hide();
    }
}

void smartmeterwindow::updTimerEvent(void) {

    struct moduleservice::cmd command;

    command.type = moduleservice::eActval;
    command.destAddr = 47;

    emit serviceCmd(&command, this);
}

void smartmeterwindow::show(void) {
    isVisible = true;
    QDialog::show();
    updTimerEvent();
    updTimer->start();
}

void smartmeterwindow::hide(void) {
    isVisible = false;
    updTimer->stop();
    QDialog::hide();
}

void smartmeterwindow::onCmdConf(const struct moduleservice::result *res, QDialog *dialog) {

    if ((dialog == this) && (res->data.state == moduleservice::eCmdOk)) {
        QString str = "Zählerstand: " +
                      QString::number(res->data.actval.data.smif.a_plus / 1000) +  " kWh";
        ui->label_aplus->setText(str);
        str = "Wirkleistung: " + QString::number(res->data.actval.data.smif.p_plus) + " W";
        ui->label_pplus->setText(str);
    }
}


void smartmeterwindow::on_pushButtonBack_clicked() {
    hide();
}
