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
    connect(this, SIGNAL(serviceCmd(const char *)), parent, SLOT(onSendServiceCmd(const char *)));
    doorbellState = false;
    ui->pushButtonDoorbell->setStyleSheet("background-color: grey");
    on_pushButtonDoorbell_pressed();
}

setupwindow::~setupwindow() {
    delete ui;
}

void setupwindow::show(void) {
    isVisible = true;
    QDialog::show();
}

void setupwindow::hide(void) {
    isVisible = false;
    QDialog::hide();
}

void setupwindow::on_pushButtonDoorbell_pressed() {

    const char *state;
    char       command[100];
    std::cout << "pressed" << std::endl;

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
