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
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(serviceCmd(const char *)), parent, SLOT(onSendServiceCmd(const char *)));
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

int ugwindow::do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen) {
    size_t i;
    int len;

    len = snprintf(pCmd, cmdLen, "-a %d -setvaldo31_do", do31Addr);

    for (i = 0; i < stateLen; i++) {
        len += snprintf(pCmd + len, cmdLen - len, " %d", *(pDoState + i));
    }
    return len;
}

void ugwindow::on_pushButtonLightStiege_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ugState.detail.lightStiege == 0) {
        doState[22] = 3; // on
    } else {
        doState[22] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightStiege->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ugwindow::on_pushButtonLightVorraum_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ugState.detail.lightVorraum == 0) {
        doState[25] = 3; // on
    } else {
        doState[25] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ugwindow::on_pushButtonLightTechnik_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ugState.detail.lightTechnik == 0) {
        doState[26] = 3; // on
    } else {
        doState[26] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightTechnik->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ugwindow::on_pushButtonLightLager_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ugState.detail.lightLager == 0) {
        doState[21] = 3; // on
    } else {
        doState[21] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightLager->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ugwindow::on_pushButtonLightFitness_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ugState.detail.lightFitness == 0) {
        doState[24] = 3; // on
    } else {
        doState[24] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightFitness->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ugwindow::on_pushButtonLightArbeit_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ugState.detail.lightArbeit == 0) {
        doState[23] = 3; // on
    } else {
        doState[23] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightArbeit->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}
