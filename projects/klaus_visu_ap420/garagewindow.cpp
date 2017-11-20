#include <stdio.h>
#include <stdint.h>
#include "garagewindow.h"
#include "ui_garagewindow.h"
#include "moduleservice.h"

garagewindow::garagewindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::garagewindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(serviceCmd(const moduleservice::cmd *, QDialog *)),
            parent, SLOT(onSendServiceCmd(const struct moduleservice::cmd *, QDialog *)));
    connect(parent, SIGNAL(cmdConf(const struct moduleservice::result *, QDialog *)),
            this, SLOT(onCmdConf(const struct moduleservice::result *, QDialog *)));

}

garagewindow::~garagewindow() {
    delete ui;
}

void garagewindow::show(void) {
    isVisible = true;
    onIoStateChanged();
    QDialog::show();
}

void garagewindow::hide(void) {
    isVisible = false;
    QDialog::hide();
}

void garagewindow::onIoStateChanged(void) {

    if (!isVisible) {
        return;
    }

    if (io->garageState.detail.light == 0) {
        ui->pushButtonLight->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLight->setStyleSheet("background-color: yellow");
    }
    if (io->garageState.detail.door == 0) {
        ui->pushButtonDoor->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonDoor->setStyleSheet("background-color: red");
    }
}

void garagewindow::on_pushButtonLight_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->garageState.detail.light == 0) {
        command.data.setvaldo31_do.setval[9] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[9] = 2; // off
    }

    ui->pushButtonLight->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLight;
    currentButtonState = (io->garageState.detail.light == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void garagewindow::onCmdConf(const struct moduleservice::result *res, QDialog *dialog) {

    if ((dialog == this) && (res->data.state == moduleservice::eCmdOk)) {
        if (currentButtonState) {
            currentButton->setStyleSheet("background-color: green");
        } else {
            currentButton->setStyleSheet("background-color: yellow");
        }
//        printf("garagewindow cmdconf %d\n", res->data.state);
    }
}

void garagewindow::on_pushButtonBack_clicked() {
    hide();
}
