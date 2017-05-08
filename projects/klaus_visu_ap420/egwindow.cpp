#include <stdio.h>
#include <stdint.h>
#include "egwindow.h"
#include "ui_egwindow.h"
#include "moduleservice.h"

egwindow::egwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::egwindow) {
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

egwindow::~egwindow() {
    delete ui;
}

void egwindow::show(void) {
    isVisible = true;
    onIoStateChanged();
    QDialog::show();
}

void egwindow::hide(void) {
    isVisible = false;
    QDialog::hide();
}

void egwindow::onIoStateChanged(void) {

    if (!isVisible) {
        return;
    }

    if (io->egState.detail.lightGang == 0) {
        ui->pushButtonLightGang->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightGang->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightEss == 0) {
        ui->pushButtonLightEss->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightEss->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightWohn == 0) {
        ui->pushButtonLightWohn->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWohn->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightWC == 0) {
        ui->pushButtonLightWC->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWC->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightVorraum == 0) {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightArbeit == 0) {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightTerrasse == 0) {
        ui->pushButtonLightTerrasse->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightTerrasse->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightWohnLese == 0) {
        ui->pushButtonLightWohnLese->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWohnLese->setStyleSheet("background-color: yellow");
    }
}

void egwindow::on_pushButtonLightGang_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 241;

    memset(&command.data, 0, sizeof(command.data));
    if (io->egState.detail.lightGang == 0) {
        command.data.setvaldo31_do.setval[25] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[25] = 2; // off
    }

    ui->pushButtonLightGang->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightGang;
    currentButtonState = (io->egState.detail.lightGang == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void egwindow::on_pushButtonLightWohn_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->egState.detail.lightWohn == 0) {
        command.data.setvaldo31_do.setval[13] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[13] = 2; // off
    }

    ui->pushButtonLightWohn->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightWohn;
    currentButtonState = (io->egState.detail.lightWohn == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void egwindow::on_pushButtonLightEss_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->egState.detail.lightEss == 0) {
        command.data.setvaldo31_do.setval[15] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[15] = 2; // off
    }

    ui->pushButtonLightEss->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightEss;
    currentButtonState = (io->egState.detail.lightEss == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void egwindow::on_pushButtonLightVorraum_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->egState.detail.lightVorraum == 0) {
        command.data.setvaldo31_do.setval[16] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[16] = 2; // off
    }

    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightVorraum;
    currentButtonState = (io->egState.detail.lightVorraum == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void egwindow::on_pushButtonLightArbeit_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 241;

    memset(&command.data, 0, sizeof(command.data));
    if (io->egState.detail.lightArbeit == 0) {
        command.data.setvaldo31_do.setval[26] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[26] = 2; // off
    }

    ui->pushButtonLightArbeit->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightArbeit;
    currentButtonState = (io->egState.detail.lightArbeit == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void egwindow::on_pushButtonLightWC_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->egState.detail.lightWC == 0) {
        command.data.setvaldo31_do.setval[17] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[17] = 2; // off
    }

    ui->pushButtonLightWC->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightWC;
    currentButtonState = (io->egState.detail.lightWC == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void egwindow::on_pushButtonLightTerrasse_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->egState.detail.lightTerrasse == 0) {
        command.data.setvaldo31_do.setval[20] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[20] = 2; // off
    }

    ui->pushButtonLightTerrasse->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightTerrasse;
    currentButtonState = (io->egState.detail.lightTerrasse == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void egwindow::on_pushButtonLightWohnLese_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->egState.detail.lightWohnLese == 0) {
        command.data.setvaldo31_do.setval[14] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[14] = 2; // off
    }

    ui->pushButtonLightWohnLese->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightWohnLese;
    currentButtonState = (io->egState.detail.lightWohnLese == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void egwindow::onCmdConf(const struct moduleservice::result *res, QDialog *dialog) {

    if ((dialog == this) && (res->data.state == moduleservice::eCmdOk)) {
        if (currentButtonState) {
            currentButton->setStyleSheet("background-color: green");
        } else {
            currentButton->setStyleSheet("background-color: yellow");
        }
//        printf("egwindow cmdconf %d\n", res->data.state);
    }
}
