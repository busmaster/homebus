#include <stdio.h>
#include <stdint.h>
#include "egwindow.h"
#include "ui_egwindow.h"

egwindow::egwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::egwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(messagePublish(const char *, const char *)),
            parent, SLOT(onMessagePublish(const char *, const char *)));
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

    if ((io->egLight & ioState::egLightBits::egLightGang) == 0) {
        ui->pushButtonLightGang->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightGang->setStyleSheet("background-color: yellow");
    }

    if ((io->egLight & ioState::egLightBits::egLightEss) == 0) {
        ui->pushButtonLightEss->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightEss->setStyleSheet("background-color: yellow");
    }

    if ((io->egLight & ioState::egLightBits::egLightWohn) == 0) {
        ui->pushButtonLightWohn->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWohn->setStyleSheet("background-color: yellow");
    }

    if ((io->egLight & ioState::egLightBits::egLightWC) == 0) {
        ui->pushButtonLightWC->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWC->setStyleSheet("background-color: yellow");
    }

    if ((io->egLight & ioState::egLightBits::egLightVorraum) == 0) {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: yellow");
    }

    if ((io->egLight & ioState::egLightBits::egLightArbeit) == 0) {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: yellow");
    }

    if ((io->egLight & ioState::egLightBits::egLightTerrasse) == 0) {
        ui->pushButtonLightTerrasse->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightTerrasse->setStyleSheet("background-color: yellow");
    }

    if ((io->egLight & ioState::egLightBits::egLightWohnLese) == 0) {
        ui->pushButtonLightWohnLese->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWohnLese->setStyleSheet("background-color: yellow");
    }

    if ((io->egLight & ioState::egLightBits::egLightEingang) == 0) {
        ui->pushButtonLightEingang->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightEingang->setStyleSheet("background-color: yellow");
    }
}


void egwindow::on_pushButtonLightGang_pressed() {

    ui->pushButtonLightGang->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/gang/licht/set", (io->egLight & ioState::egLightBits::egLightGang) ? "0" : "1");
}

void egwindow::on_pushButtonLightWohn_pressed() {

    ui->pushButtonLightWohn->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/wohn/licht/set", (io->egLight & ioState::egLightBits::egLightWohn) ? "0" : "1");
}

void egwindow::on_pushButtonLightEss_pressed() {

    ui->pushButtonLightEss->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/ess/licht/set", (io->egLight & ioState::egLightBits::egLightEss) ? "0" : "1");
}

void egwindow::on_pushButtonLightVorraum_pressed() {

    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/vorraum/licht/set", (io->egLight & ioState::egLightBits::egLightVorraum) ? "0" : "1");
}

void egwindow::on_pushButtonLightArbeit_pressed() {

    ui->pushButtonLightArbeit->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/arbeit/licht/set", (io->egLight & ioState::egLightBits::egLightArbeit) ? "0" : "1");
}

void egwindow::on_pushButtonLightWC_pressed() {

    ui->pushButtonLightWC->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/wc/licht/set", (io->egLight & ioState::egLightBits::egLightWC) ? "0" : "1");
}

void egwindow::on_pushButtonLightTerrasse_pressed() {

    ui->pushButtonLightTerrasse->setStyleSheet("background-color: grey");
    emit messagePublish("home/terrasse/licht/set", (io->egLight & ioState::egLightBits::egLightTerrasse) ? "0" : "1");
}

void egwindow::on_pushButtonLightWohnLese_pressed() {

    ui->pushButtonLightWohnLese->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/wohn/lesen/licht/set", (io->egLight & ioState::egLightBits::egLightWohnLese) ? "0" : "1");
}

void egwindow::on_pushButtonLightEingang_pressed() {

    ui->pushButtonLightEingang->setStyleSheet("background-color: grey");
    emit messagePublish("home/eingang/licht/set", (io->egLight & ioState::egLightBits::egLightEingang) ? "0" : "1");
}

void egwindow::on_pushButtonBack_clicked() {
    hide();
}
