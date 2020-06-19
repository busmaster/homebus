#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include "setupwindow.h"
#include "ui_setupwindow.h"

setupwindow::setupwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::setupwindow) {
    ui->setupUi(this);
    isVisible = false;
    io = state;
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(messagePublish(const char *, const char *)),
            parent, SLOT(onMessagePublish(const char *, const char *)));
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
    if ((io->sockets & ioState::socketBits::socketInternet) == 1) {
        ui->pushButtonInternetEin->setStyleSheet("background-color: green");
        ui->pushButtonInternetAus->setStyleSheet("background-color: grey");
    } else {
        ui->pushButtonInternetEin->setStyleSheet("background-color: grey");
        ui->pushButtonInternetAus->setStyleSheet("background-color: green");
    }

    if (io->var_glocke_disable == 0) {
        ui->pushButtonGlockeAus->setStyleSheet("background-color: grey");
        ui->pushButtonGlockeEin->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonGlockeAus->setStyleSheet("background-color: green");
        ui->pushButtonGlockeEin->setStyleSheet("background-color: grey");
    }

    if (io->var_mode_LightEingang == 0) { // auto
        ui->pushButtonLichtEingangEin->setStyleSheet("background-color: grey");
        ui->pushButtonLichtEingangAus->setStyleSheet("background-color: grey");
        ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: green");
    } else if (io->var_mode_LightEingang == 1) { // off
        ui->pushButtonLichtEingangEin->setStyleSheet("background-color: grey");
        ui->pushButtonLichtEingangAus->setStyleSheet("background-color: green");
        ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: grey");
    } else if (io->var_mode_LightEingang == 2) { // on
        ui->pushButtonLichtEingangEin->setStyleSheet("background-color: green");
        ui->pushButtonLichtEingangAus->setStyleSheet("background-color: grey");
        ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: grey");
    }
}

void setupwindow::on_pushButtonGlockeAus_pressed() {

    if (io->var_glocke_disable == 0) {
        ui->pushButtonGlockeAus->setStyleSheet("background-color: darkGreen");
        ui->pushButtonGlockeEin->setStyleSheet("background-color: grey");
        emit messagePublish("home/glocke/disable/set", "01"); // variable as hex number
    }
}

void setupwindow::on_pushButtonGlockeEin_pressed() {

    if (io->var_glocke_disable == 1) {
        ui->pushButtonGlockeEin->setStyleSheet("background-color: darkGreen");
        ui->pushButtonGlockeAus->setStyleSheet("background-color: grey");
        emit messagePublish("home/glocke/disable/set", "00"); // variable as hex number
    }
}

void setupwindow::on_pushButtonInternetAus_pressed() {

    ui->pushButtonInternetAus->setStyleSheet("background-color: darkGreen");
    ui->pushButtonInternetEin->setStyleSheet("background-color: grey");
    emit messagePublish("home/internet/set", "0");
}

void setupwindow::on_pushButtonInternetEin_pressed() {

    ui->pushButtonInternetEin->setStyleSheet("background-color: darkGreen");
    ui->pushButtonInternetAus->setStyleSheet("background-color: grey");
    emit messagePublish("home/internet/set", "1");
}

void setupwindow::on_pushButtonLichtEingangEin_pressed() {

    if (io->var_mode_LightEingang != 2) {
        ui->pushButtonLichtEingangEin->setStyleSheet("background-color: darkGreen");
        ui->pushButtonLichtEingangAus->setStyleSheet("background-color: grey");
        ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: grey");
        emit messagePublish("home/eingang/licht/mode/set", "02"); // variable as hex number
    }
}

void setupwindow::on_pushButtonLichtEingangAus_pressed() {

    if (io->var_mode_LightEingang != 1) {
        ui->pushButtonLichtEingangAus->setStyleSheet("background-color: darkGreen");
        ui->pushButtonLichtEingangEin->setStyleSheet("background-color: grey");
        ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: grey");
        emit messagePublish("home/eingang/licht/mode/set", "01"); // variable as hex number
    }
}

void setupwindow::on_pushButtonLichtEingangAuto_pressed() {

    if (io->var_mode_LightEingang != 0) {
        ui->pushButtonLichtEingangAuto->setStyleSheet("background-color: darkGreen");
        ui->pushButtonLichtEingangAus->setStyleSheet("background-color: grey");
        ui->pushButtonLichtEingangEin->setStyleSheet("background-color: grey");
        emit messagePublish("home/eingang/licht/mode/set", "00"); // variable as hex number
    }
}

void setupwindow::on_pushButtonBack_clicked() {
    hide();
}
