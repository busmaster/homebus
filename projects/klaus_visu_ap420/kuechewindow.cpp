#include <stdio.h>
#include <stdint.h>
#include "kuechewindow.h"
#include "ui_kuechewindow.h"
#include "moduleservice.h"

kuechewindow::kuechewindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::kuechewindow) {
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

kuechewindow::~kuechewindow(){
    delete ui;
}

void kuechewindow::show(void) {
    isVisible = true;
    onIoStateChanged();
    QDialog::show();
}

void kuechewindow::hide(void) {
    isVisible = false;
    QDialog::hide();
}

void kuechewindow::onIoStateChanged(void) {

    if (!isVisible) {
        return;
    }

    if (io->kuecheState.detail.light == 0) {
        ui->pushButtonLight->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLight->setStyleSheet("background-color: yellow");
    }

    if (io->kuecheState.detail.lightWand == 0) {
        ui->pushButtonLightWand->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWand->setStyleSheet("background-color: yellow");
    }

    if (io->kuecheState.detail.lightDunstabzug == 0) {
        ui->pushButtonLightDunstabzug->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightDunstabzug->setStyleSheet("background-color: yellow");
    }

    if (io->kuecheState.detail.lightAbwasch == 0) {
        ui->pushButtonLightAbwasch->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightAbwasch->setStyleSheet("background-color: yellow");
    }

    if (io->kuecheState.detail.lightGeschirrspueler == 0) {
        ui->pushButtonLightGeschirrspueler->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightGeschirrspueler->setStyleSheet("background-color: yellow");
    }

    if (io->kuecheState.detail.lightKaffee == 0) {
        ui->pushButtonLightKaffee->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightKaffee->setStyleSheet("background-color: yellow");
    }

    if (io->kuecheState.detail.lightSpeis == 0) {
        ui->pushButtonLightSpeis->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightSpeis->setStyleSheet("background-color: yellow");
    }
}

void kuechewindow::sendDo31Cmd(quint8 destAddr, quint8 doNr, QPushButton *button, bool currState) {

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

void kuechewindow::on_pushButtonLight_pressed() {

    sendDo31Cmd(240, 19, ui->pushButtonLight, io->kuecheState.detail.light != 0);
}

void kuechewindow::on_pushButtonLightWand_pressed() {

    sendDo31Cmd(241, 30, ui->pushButtonLightWand, io->kuecheState.detail.lightWand != 0);
}

void kuechewindow::on_pushButtonLightSpeis_pressed() {

    sendDo31Cmd(241, 24, ui->pushButtonLightSpeis, io->kuecheState.detail.lightSpeis != 0);
}

void kuechewindow::sendPwm4Cmd(quint8 addr, quint8 pwmNr, QPushButton *button, bool currState) {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvalpwm4;
    command.destAddr = addr;

    memset(command.data.setvalpwm4.set, 0, sizeof(command.data.setvalpwm4.set));
    memset(command.data.setvalpwm4.pwm, 0, sizeof(command.data.setvalpwm4.pwm));

    currentButtonState = currState;
    if (currState) {
        command.data.setvalpwm4.set[pwmNr] = 3; // off
    } else {
        command.data.setvalpwm4.set[pwmNr] = 1; // on
    }

    button->setStyleSheet("background-color: grey");
    currentButton = button;

    emit serviceCmd(&command, this);
}



void kuechewindow::on_pushButtonLightGeschirrspueler_pressed() {

    sendPwm4Cmd(239, 0, ui->pushButtonLightGeschirrspueler, io->kuecheState.detail.lightGeschirrspueler != 0);
}

void kuechewindow::on_pushButtonLightAbwasch_pressed() {

    sendPwm4Cmd(239, 1, ui->pushButtonLightAbwasch, io->kuecheState.detail.lightAbwasch != 0);
}

void kuechewindow::on_pushButtonLightKaffee_pressed() {

    sendPwm4Cmd(239, 2, ui->pushButtonLightKaffee, io->kuecheState.detail.lightKaffee != 0);
}

void kuechewindow::on_pushButtonLightDunstabzug_pressed() {

    sendPwm4Cmd(239, 3, ui->pushButtonLightDunstabzug, io->kuecheState.detail.lightDunstabzug != 0);
}

void kuechewindow::on_verticalSlider_valueChanged(int value) {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvalpwm4;
    command.destAddr = 239;

    command.data.setvalpwm4.set[0] = 2;
    command.data.setvalpwm4.set[1] = 2;
    command.data.setvalpwm4.set[2] = 2;
    command.data.setvalpwm4.set[3] = 2;
    command.data.setvalpwm4.pwm[0] = value;
    command.data.setvalpwm4.pwm[1] = value;
    command.data.setvalpwm4.pwm[2] = value;
    command.data.setvalpwm4.pwm[3] = value;

    currentButton = 0;

    emit serviceCmd(&command, this);
}

void kuechewindow::onCmdConf(const struct moduleservice::result *res, QDialog *dialog) {

    if ((dialog == this) && currentButton && (res->data.state == moduleservice::eCmdOk))  {
        if (currentButtonState) {
            currentButton->setStyleSheet("background-color: green");
        } else {
            currentButton->setStyleSheet("background-color: yellow");
        }
//        printf("kuechewindow cmdconf %d\n", res->data.state);
    }
}
