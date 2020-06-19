#include <stdio.h>
#include <stdint.h>
#include "garagewindow.h"
#include "ui_garagewindow.h"

garagewindow::garagewindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::garagewindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(messagePublish(const char *, const char *)),
            parent, SLOT(onMessagePublish(const char *, const char *)));
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

    if (!(io->garage & ioState::garageBits::garageLight)) {
        ui->pushButtonLight->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLight->setStyleSheet("background-color: yellow");
    }
    if (io->garage & ioState::garageBits::garageDoorClosed) {
        ui->pushButtonDoor->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonDoor->setStyleSheet("background-color: red");
    }
}

void garagewindow::on_pushButtonLight_pressed() {

    ui->pushButtonLight->setStyleSheet("background-color: grey");
    emit messagePublish("home/garage/licht/set", (io->garage & ioState::garageBits::garageLight) ? "0" : "1");
}

void garagewindow::on_pushButtonBack_clicked() {
    hide();
}
