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
    connect(this, SIGNAL(serviceCmd(const char *)), parent, SLOT(onSendServiceCmd(const char *)));
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    doorbellState = false;
    ui->pushButtonDoorbell->setStyleSheet("background-color: grey");
    on_pushButtonDoorbell_pressed();
    ui->pushButtonInternet->setStyleSheet("background-color: grey");
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
        ui->pushButtonInternet->setText(QString("Internet\nEIN"));
    } else {
        ui->pushButtonInternet->setText(QString("Internet\nAUS"));
    }
}

int setupwindow::do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen) {
    size_t i;
    int len;

    len = snprintf(pCmd, cmdLen, "-a %d -setvaldo31_do", do31Addr);

    for (i = 0; i < stateLen; i++) {
        len += snprintf(pCmd + len, cmdLen - len, " %d", *(pDoState + i));
    }
    return len;
}



void setupwindow::on_pushButtonDoorbell_pressed() {

    const char *state;
    char       command[100];
//    std::cout << "pressed" << std::endl;

    if (doorbellState) {
        ui->pushButtonDoorbell->setText(QString("Glocke\nAUS"));
        state = "0";
        doorbellState = false;
    } else {
        ui->pushButtonDoorbell->setText(QString("Glocke\nEIN"));
        state = "1";
        doorbellState = true;
    }

    snprintf(command, sizeof(command), "-a 240 -o 100 -switchstate %s", state);

    emit serviceCmd(command);
}

void setupwindow::on_pushButtonInternet_clicked() {

    char    command[100];
    uint8_t doState[31];

    ui->pushButtonInternet->setText(QString("Internet\n"));
    memset(doState, 0, sizeof(doState));
    if (io->socket_1) {
        doState[27] = 3; // on
    } else {
        doState[27] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    emit serviceCmd(command);
}
