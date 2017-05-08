#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include "setupwindow.h"
#include "ui_setupwindow.h"
#include "moduleservice.h"

setupwindow::setupwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::setupwindow) {
    ui->setupUi(this);
    isVisible = false;
    io = state;
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(serviceCmd(const moduleservice::cmd *, QDialog *)),
            parent, SLOT(onSendServiceCmd(const struct moduleservice::cmd *, QDialog *)));
    connect(parent, SIGNAL(cmdConf(const struct moduleservice::result *, QDialog *)),
            this, SLOT(onCmdConf(const struct moduleservice::result *, QDialog *)));

    doorbellState = false;
    ui->pushButtonDoorbell->setStyleSheet("background-color: grey");
    on_pushButtonDoorbell_pressed();
    ui->pushButtonInternet->setStyleSheet("background-color: grey");
}

setupwindow::~setupwindow() {
    delete ui;
}

void setupwindow::show(void) {
    isVisible = true;
    onIoStateChanged();
    QDialog::show();
}

void setupwindow::hide(void) {
    isVisible = false;
    QDialog::hide();
}

void setupwindow::onIoStateChanged(void) {

    if (!isVisible) {
        return;
    }

    if (io->socket_1) {
        ui->pushButtonInternet->setText(QString("Internet\nEIN"));
    } else {
        ui->pushButtonInternet->setText(QString("Internet\nAUS"));
    }
}

void setupwindow::on_pushButtonDoorbell_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSwitchstate;
    command.destAddr = 240;
    command.ownAddr = 100;

    if (doorbellState) {
        command.data.switchstate = 0;
    } else {
        command.data.switchstate = 1;
    }

    ui->pushButtonDoorbell->setText(QString("Glocke\n"));
    currentButton = ui->pushButtonDoorbell;

    emit serviceCmd(&command, this);
}

void setupwindow::on_pushButtonInternet_clicked() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->socket_1) {
        command.data.setvaldo31_do.setval[27] = 3; // on
        currentButtonState = false;
    } else {
        command.data.setvaldo31_do.setval[27] = 2; // off
        currentButtonState = true;
    }
    ui->pushButtonInternet->setText(QString("Internet\n"));
    currentButton = ui->pushButtonInternet;

    emit serviceCmd(&command, this);
}

void setupwindow::onCmdConf(const struct moduleservice::result *res, QDialog *dialog) {

    if ((dialog == this) && (res->data.state == moduleservice::eCmdOk)) {
        if (currentButton == ui->pushButtonInternet) {
            if (currentButtonState) {
                currentButton->setText(QString("Internet\nEIN"));
            } else {
                currentButton->setText(QString("Internet\nAUS"));
            }
        } else if (currentButton == ui->pushButtonDoorbell) {
            if (doorbellState) {
                currentButton->setText(QString("Glocke\nAUS"));
                doorbellState = false;
            } else {
                currentButton->setText(QString("Glocke\nEIN"));
                doorbellState = true;
            }
        }
//        printf("setupwindow cmdconf %d\n", res->data.state);
    }
}
