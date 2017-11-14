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

    if (io->egState.detail.lightEingang == 0) {
        ui->pushButtonLightEingang->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightEingang->setStyleSheet("background-color: yellow");
    }
}

void egwindow::sendDo31Cmd(quint8 destAddr, quint8 doNr, QPushButton *button, bool currState) {

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

void egwindow::on_pushButtonLightGang_pressed() {

    sendDo31Cmd(241, 25, ui->pushButtonLightGang, io->egState.detail.lightGang != 0);
}

void egwindow::on_pushButtonLightWohn_pressed() {

    sendDo31Cmd(240, 13, ui->pushButtonLightWohn, io->egState.detail.lightWohn != 0);
}

void egwindow::on_pushButtonLightEss_pressed() {

    sendDo31Cmd(240, 15, ui->pushButtonLightEss, io->egState.detail.lightEss != 0);
}

void egwindow::on_pushButtonLightVorraum_pressed() {

    sendDo31Cmd(240, 16, ui->pushButtonLightVorraum, io->egState.detail.lightVorraum != 0);
}

void egwindow::on_pushButtonLightArbeit_pressed() {

    sendDo31Cmd(241, 26, ui->pushButtonLightArbeit, io->egState.detail.lightArbeit != 0);
}

void egwindow::on_pushButtonLightWC_pressed() {

    sendDo31Cmd(240, 17, ui->pushButtonLightWC, io->egState.detail.lightWC != 0);
}

void egwindow::on_pushButtonLightTerrasse_pressed() {

    sendDo31Cmd(240, 20, ui->pushButtonLightTerrasse, io->egState.detail.lightTerrasse != 0);
}

void egwindow::on_pushButtonLightWohnLese_pressed() {

    sendDo31Cmd(240, 14, ui->pushButtonLightWohnLese, io->egState.detail.lightWohnLese != 0);
}

void egwindow::on_pushButtonLightEingang_pressed() {
    sendDo31Cmd(240, 30, ui->pushButtonLightEingang, io->egState.detail.lightEingang != 0);
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

