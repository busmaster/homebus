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
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(serviceCmd(const char *)), parent, SLOT(onSendServiceCmd(const char *)));
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

int garagewindow::do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen) {
    size_t i;
    int len;

    len = snprintf(pCmd, cmdLen, "-a %d -setvaldo31_do", do31Addr);

    for (i = 0; i < stateLen; i++) {
        len += snprintf(pCmd + len, cmdLen - len, " %d", *(pDoState + i));
    }
    return len;
}

void garagewindow::on_pushButtonLight_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->garageState.detail.light == 0) {
        doState[9] = 3; // on
    } else {
        doState[9] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLight->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}
