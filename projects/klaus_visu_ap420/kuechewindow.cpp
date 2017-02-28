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
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(serviceCmd(const char *)), parent, SLOT(onSendServiceCmd(const char *)));

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

int kuechewindow::do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen) {
    size_t i;
    int len;

    len = snprintf(pCmd, cmdLen, "-a %d -setvaldo31_do", do31Addr);

    for (i = 0; i < stateLen; i++) {
        len += snprintf(pCmd + len, cmdLen - len, " %d", *(pDoState + i));
    }
    return len;
}

int kuechewindow::pwm4Cmd(int pwm4Addr, uint16_t *pwmVal, uint8_t *set, char *pCmd, size_t cmdLen) {
    size_t i;
    int len;

    len = snprintf(pCmd, cmdLen, "-a %d -setvalpwm4", pwm4Addr);

    for (i = 0; i < 4; i++) {
        len += snprintf(pCmd + len, cmdLen - len, " %d %d", *(set + i), *(pwmVal + i));
    }
    return len;
}

void kuechewindow::on_pushButtonLight_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->kuecheState.detail.light == 0) {
        doState[19] = 3; // on
    } else {
        doState[19] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLight->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void kuechewindow::on_pushButtonLightWand_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->kuecheState.detail.lightWand == 0) {
        doState[30] = 3; // on
    } else {
        doState[30] = 2; // off
    }
    do31Cmd(241, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightWand->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void kuechewindow::on_pushButtonLightSpeis_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->kuecheState.detail.lightSpeis == 0) {
        doState[24] = 3; // on
    } else {
        doState[24] = 2; // off
    }
    do31Cmd(241, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightSpeis->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void kuechewindow::on_pushButtonLightGeschirrspueler_pressed() {
    char     command[100];
    uint16_t pwm[4];
    uint8_t  set[4];

    memset(pwm, 0, sizeof(pwm));
    memset(set, 0, sizeof(set));
    if (io->kuecheState.detail.lightGeschirrspueler == 0) {
        set[0] = 1; // on
    } else {
        set[0] = 3; // off
    }
    pwm4Cmd(239, pwm, set, command, sizeof(command));
    ui->pushButtonLightGeschirrspueler->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void kuechewindow::on_pushButtonLightAbwasch_pressed() {
    char     command[100];
    uint16_t pwm[4];
    uint8_t  set[4];

    memset(pwm, 0, sizeof(pwm));
    memset(set, 0, sizeof(set));
    if (io->kuecheState.detail.lightAbwasch == 0) {
        set[1] = 1; // on
    } else {
        set[1] = 3; // off
    }
    pwm4Cmd(239, pwm, set, command, sizeof(command));
    ui->pushButtonLightAbwasch->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void kuechewindow::on_pushButtonLightKaffee_pressed() {
    char     command[100];
    uint16_t pwm[4];
    uint8_t  set[4];

    memset(pwm, 0, sizeof(pwm));
    memset(set, 0, sizeof(set));
    if (io->kuecheState.detail.lightKaffee == 0) {
        set[2] = 1; // on
    } else {
        set[2] = 3; // off
    }
    pwm4Cmd(239, pwm, set, command, sizeof(command));
    ui->pushButtonLightKaffee->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void kuechewindow::on_pushButtonLightDunstabzug_pressed() {
    char     command[100];
    uint16_t pwm[4];
    uint8_t  set[4];

    memset(pwm, 0, sizeof(pwm));
    memset(set, 0, sizeof(set));
    if (io->kuecheState.detail.lightDunstabzug == 0) {
        set[3] = 1; // on
    } else {
        set[3] = 3; // off
    }
    pwm4Cmd(239, pwm, set, command, sizeof(command));
    ui->pushButtonLightDunstabzug->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void kuechewindow::on_verticalSlider_valueChanged(int value)
{
    char     command[100];
    uint16_t pwm[4];
    uint8_t  set[4];

    pwm[0] = value;
    pwm[1] = value;
    pwm[2] = value;
    pwm[3] = value;
    set[0] = 2;
    set[1] = 2;
    set[2] = 2;
    set[3] = 2;
    pwm4Cmd(239, pwm, set, command, sizeof(command));

    emit serviceCmd(command);
}
