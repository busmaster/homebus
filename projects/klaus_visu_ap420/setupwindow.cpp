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

//    on_pushButtonInternetEin_pressed();
//    on_pushButtonGlockeEin_pressed();
//    on_pushButtonLichtEingangAuto_pressed();
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
        ui->pushButtonInternetEin->setStyleSheet("background-color: green");
        ui->pushButtonInternetAus->setStyleSheet("background-color: grey");
    } else {
        ui->pushButtonInternetEin->setStyleSheet("background-color: grey");
        ui->pushButtonInternetAus->setStyleSheet("background-color: green");
    }
}

void setupwindow::onCmdConf(const struct moduleservice::result *res, QDialog *dialog) {

    struct moduleservice::cmd command;
    if ((dialog == this) && (res->data.state == moduleservice::eCmdOk)) {
        switch (currentButton) {
        case eCurrInternetAus:
            ui->pushButtonInternetAus->setStyleSheet("background-color: green");
            break;
        case eCurrInternetEin:
            ui->pushButtonInternetEin->setStyleSheet("background-color: green");
            break;
        case eCurrGlockeAus:
            ui->pushButtonGlockeAus->setStyleSheet("background-color: green");
            break;
        case eCurrGlockeEin:
            ui->pushButtonGlockeEin->setStyleSheet("background-color: green");
            break;
        case eCurrLichtEingangEin:
            ui->pushButtonLichtEingangEin->setStyleSheet("background-color: green");
            command.type = moduleservice::eSwitchstate;
            command.destAddr = 240;
            command.ownAddr = 102;
            command.data.switchstate = 0;
            currentButton = eCurrNone;
            emit serviceCmd(&command, this);
            break;
        case eCurrLichtEingangAus:
            ui->pushButtonLichtEingangAus->setStyleSheet("background-color: green");
            command.type = moduleservice::eSwitchstate;
            command.destAddr = 240;
            command.ownAddr = 102;
            command.data.switchstate = 0;
            currentButton = eCurrNone;
            emit serviceCmd(&command, this);
            break;
        case eCurrLichtEingangAuto:
            ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: green");
            command.type = moduleservice::eSwitchstate;
            command.destAddr = 240;
            command.ownAddr = 101;
            command.data.switchstate = 0;
            currentButton = eCurrNone;
            emit serviceCmd(&command, this);
            break;
        default:
            break;
        }

//        printf("setupwindow cmdconf %d\n", res->data.state);
    }
}


void setupwindow::on_pushButtonGlockeAus_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSwitchstate;
    command.destAddr = 240;
    command.ownAddr = 100;
    command.data.switchstate = 0;
    ui->pushButtonGlockeAus->setStyleSheet("background-color: darkGreen");
    ui->pushButtonGlockeEin->setStyleSheet("background-color: grey");
    currentButton = eCurrGlockeAus;

    emit serviceCmd(&command, this);
}

void setupwindow::on_pushButtonGlockeEin_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSwitchstate;
    command.destAddr = 240;
    command.ownAddr = 100;
    command.data.switchstate = 1;
    ui->pushButtonGlockeEin->setStyleSheet("background-color: darkGreen");
    ui->pushButtonGlockeAus->setStyleSheet("background-color: grey");
    currentButton = eCurrGlockeEin;

    emit serviceCmd(&command, this);
}

void setupwindow::on_pushButtonInternetAus_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;
    memset(&command.data, 0, sizeof(command.data));
    command.data.setvaldo31_do.setval[27] = 3; // on, relay normally closed
    ui->pushButtonInternetAus->setStyleSheet("background-color: darkGreen");
    ui->pushButtonInternetEin->setStyleSheet("background-color: grey");
    currentButton = eCurrInternetAus;

    emit serviceCmd(&command, this);
}

void setupwindow::on_pushButtonInternetEin_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;
    memset(&command.data, 0, sizeof(command.data));
    command.data.setvaldo31_do.setval[27] = 2; // off, relay normally closed
    ui->pushButtonInternetEin->setStyleSheet("background-color: darkGreen");
    ui->pushButtonInternetAus->setStyleSheet("background-color: grey");
    currentButton = eCurrInternetEin;

    emit serviceCmd(&command, this);
}

void setupwindow::on_pushButtonLichtEingangEin_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSwitchstate;
    command.destAddr = 240;
    command.ownAddr = 102;
    command.data.switchstate = 1;
    ui->pushButtonLichtEingangEin->setStyleSheet("background-color: darkGreen");
    ui->pushButtonLichtEingangAus->setStyleSheet("background-color: grey");
    ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: grey");
    currentButton = eCurrLichtEingangEin;

    emit serviceCmd(&command, this);
}

void setupwindow::on_pushButtonLichtEingangAus_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSwitchstate;
    command.destAddr = 240;
    command.ownAddr = 102;
    command.data.switchstate = 2;
    ui->pushButtonLichtEingangAus->setStyleSheet("background-color: darkGreen");
    ui->pushButtonLichtEingangEin->setStyleSheet("background-color: grey");
    ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: grey");
    currentButton = eCurrLichtEingangAus;

    emit serviceCmd(&command, this);
}

void setupwindow::on_pushButtonLichtEingangAuto_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSwitchstate;
    command.destAddr = 240;
    command.ownAddr = 101;
    command.data.switchstate = 2;
    ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: darkGreen");
    ui->pushButtonLichtEingangAus->setStyleSheet("background-color: grey");
    ui->pushButtonLichtEingangEin->setStyleSheet("background-color: grey");
    currentButton = eCurrLichtEingangAuto;

    emit serviceCmd(&command, this);
}

void setupwindow::on_pushButtonBack_clicked() {
    hide();
}
