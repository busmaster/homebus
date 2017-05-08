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
/*
int kuechewindow::pwm4Cmd(int pwm4Addr, uint16_t *pwmVal, uint8_t *set, char *pCmd, size_t cmdLen) {
    size_t i;
    int len;

    len = snprintf(pCmd, cmdLen, "-a %d -setvalpwm4", pwm4Addr);

    for (i = 0; i < 4; i++) {
        len += snprintf(pCmd + len, cmdLen - len, " %d %d", *(set + i), *(pwmVal + i));
    }
    return len;
}
*/
void kuechewindow::on_pushButtonLight_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->kuecheState.detail.light == 0) {
        command.data.setvaldo31_do.setval[19] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[19] = 2; // off
    }

    ui->pushButtonLight->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLight;
    currentButtonState = (io->kuecheState.detail.light == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void kuechewindow::on_pushButtonLightWand_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 241;

    memset(&command.data, 0, sizeof(command.data));
    if (io->kuecheState.detail.lightWand == 0) {
        command.data.setvaldo31_do.setval[30] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[30] = 2; // off
    }

    ui->pushButtonLightWand->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightWand;
    currentButtonState = (io->kuecheState.detail.lightWand == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void kuechewindow::on_pushButtonLightSpeis_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 241;

    memset(&command.data, 0, sizeof(command.data));
    if (io->kuecheState.detail.lightSpeis == 0) {
        command.data.setvaldo31_do.setval[24] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[24] = 2; // off
    }

    ui->pushButtonLightSpeis->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightSpeis;
    currentButtonState = (io->kuecheState.detail.lightSpeis == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void kuechewindow::on_pushButtonLightGeschirrspueler_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvalpwm4;
    command.destAddr = 239;

    memset(command.data.setvalpwm4.set, 0, sizeof(command.data.setvalpwm4.set));
    memset(command.data.setvalpwm4.pwm, 0, sizeof(command.data.setvalpwm4.pwm));

    if (io->kuecheState.detail.lightGeschirrspueler == 0) {
        command.data.setvalpwm4.set[0] = 1; // on
    } else {
        command.data.setvalpwm4.set[0] = 3; // off
    }

    ui->pushButtonLightGeschirrspueler->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightGeschirrspueler;
    currentButtonState = (io->kuecheState.detail.lightGeschirrspueler == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void kuechewindow::on_pushButtonLightAbwasch_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvalpwm4;
    command.destAddr = 239;

    memset(command.data.setvalpwm4.set, 0, sizeof(command.data.setvalpwm4.set));
    memset(command.data.setvalpwm4.pwm, 0, sizeof(command.data.setvalpwm4.pwm));

    if (io->kuecheState.detail.lightAbwasch == 0) {
        command.data.setvalpwm4.set[1] = 1; // on
    } else {
        command.data.setvalpwm4.set[1] = 3; // off
    }

    ui->pushButtonLightAbwasch->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightAbwasch;
    currentButtonState = (io->kuecheState.detail.lightAbwasch == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void kuechewindow::on_pushButtonLightKaffee_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvalpwm4;
    command.destAddr = 239;

    memset(command.data.setvalpwm4.set, 0, sizeof(command.data.setvalpwm4.set));
    memset(command.data.setvalpwm4.pwm, 0, sizeof(command.data.setvalpwm4.pwm));

    if (io->kuecheState.detail.lightKaffee == 0) {
        command.data.setvalpwm4.set[2] = 1; // on
    } else {
        command.data.setvalpwm4.set[2] = 3; // off
    }

    ui->pushButtonLightKaffee->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightKaffee;
    currentButtonState = (io->kuecheState.detail.lightKaffee == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void kuechewindow::on_pushButtonLightDunstabzug_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvalpwm4;
    command.destAddr = 239;

    memset(command.data.setvalpwm4.set, 0, sizeof(command.data.setvalpwm4.set));
    memset(command.data.setvalpwm4.pwm, 0, sizeof(command.data.setvalpwm4.pwm));

    if (io->kuecheState.detail.lightDunstabzug == 0) {
        command.data.setvalpwm4.set[3] = 1; // on
    } else {
        command.data.setvalpwm4.set[3] = 3; // off
    }

    ui->pushButtonLightDunstabzug->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightDunstabzug;
    currentButtonState = (io->kuecheState.detail.lightDunstabzug == 0) ? false : true;

    emit serviceCmd(&command, this);
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
