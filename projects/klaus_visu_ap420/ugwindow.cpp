#include <stdio.h>
#include <stdint.h>
#include "ugwindow.h"
#include "ui_ugwindow.h"
#include "moduleservice.h"

ugwindow::ugwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::ugwindow) {
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

ugwindow::~ugwindow() {
    delete ui;
}

void ugwindow::show(void) {
    isVisible = true;
    onIoStateChanged();
    QDialog::show();
}

void ugwindow::hide(void) {
    isVisible = false;
    QDialog::hide();
}

void ugwindow::onIoStateChanged(void) {

    if (!isVisible) {
        return;
    }

    if (io->ugState.detail.lightVorraum == 0) {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightLager == 0) {
        ui->pushButtonLightLager->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightLager->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightTechnik == 0) {
        ui->pushButtonLightTechnik->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightTechnik->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightArbeit == 0) {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightFitness == 0) {
        ui->pushButtonLightFitness->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightFitness->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightStiege == 0) {
        ui->pushButtonLightStiege->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightStiege->setStyleSheet("background-color: yellow");
    }

}

void ugwindow::on_pushButtonLightStiege_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ugState.detail.lightStiege == 0) {
        command.data.setvaldo31_do.setval[22] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[22] = 2; // off
    }

    ui->pushButtonLightStiege->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightStiege;
    currentButtonState = (io->ugState.detail.lightStiege == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ugwindow::on_pushButtonLightVorraum_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ugState.detail.lightVorraum == 0) {
        command.data.setvaldo31_do.setval[25] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[25] = 2; // off
    }

    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightVorraum;
    currentButtonState = (io->ugState.detail.lightVorraum == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ugwindow::on_pushButtonLightTechnik_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ugState.detail.lightTechnik == 0) {
        command.data.setvaldo31_do.setval[26] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[26] = 2; // off
    }

    ui->pushButtonLightTechnik->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightTechnik;
    currentButtonState = (io->ugState.detail.lightTechnik == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ugwindow::on_pushButtonLightLager_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ugState.detail.lightLager == 0) {
        command.data.setvaldo31_do.setval[21] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[21] = 2; // off
    }

    ui->pushButtonLightLager->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightLager;
    currentButtonState = (io->ugState.detail.lightLager == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ugwindow::on_pushButtonLightFitness_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ugState.detail.lightFitness == 0) {
        command.data.setvaldo31_do.setval[24] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[24] = 2; // off
    }

    ui->pushButtonLightFitness->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightFitness;
    currentButtonState = (io->ugState.detail.lightFitness == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ugwindow::on_pushButtonLightArbeit_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ugState.detail.lightArbeit == 0) {
        command.data.setvaldo31_do.setval[23] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[23] = 2; // off
    }

    ui->pushButtonLightArbeit->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightArbeit;
    currentButtonState = (io->ugState.detail.lightArbeit == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ugwindow::onCmdConf(const struct moduleservice::result *res, QDialog *dialog) {

    if ((dialog == this) && (res->data.state == moduleservice::eCmdOk)) {
        if (currentButtonState) {
            currentButton->setStyleSheet("background-color: green");
        } else {
            currentButton->setStyleSheet("background-color: yellow");
        }
//        printf("ugwindow cmdconf %d\n", res->data.state);
    }
}
