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

void ugwindow::sendDo31Cmd(quint8 destAddr, quint8 doNr, QPushButton *button, bool currState) {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = destAddr;

    memset(&command.data, 0, sizeof(command.data));
    currentButtonState = currState;
    if (currState) {
        command.data.setvaldo31_do.setval[doNr] = 2; // off
    } else {
        command.data.setvaldo31_do.setval[doNr] = 3; // on
    }

    button->setStyleSheet("background-color: grey");
    currentButton = button;

    emit serviceCmd(&command, this);
}

void ugwindow::on_pushButtonLightStiege_pressed() {

    sendDo31Cmd(240, 22, ui->pushButtonLightStiege, io->ugState.detail.lightStiege != 0);
}

void ugwindow::on_pushButtonLightVorraum_pressed() {

    sendDo31Cmd(240, 25, ui->pushButtonLightVorraum, io->ugState.detail.lightVorraum != 0);
}

void ugwindow::on_pushButtonLightTechnik_pressed() {

    sendDo31Cmd(240, 26, ui->pushButtonLightTechnik, io->ugState.detail.lightTechnik != 0);
}

void ugwindow::on_pushButtonLightLager_pressed() {

    sendDo31Cmd(240, 21, ui->pushButtonLightLager, io->ugState.detail.lightLager != 0);
}

void ugwindow::on_pushButtonLightFitness_pressed() {

    sendDo31Cmd(240, 24, ui->pushButtonLightFitness, io->ugState.detail.lightFitness != 0);
}

void ugwindow::on_pushButtonLightArbeit_pressed() {

    sendDo31Cmd(240, 23, ui->pushButtonLightArbeit, io->ugState.detail.lightArbeit != 0);
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
