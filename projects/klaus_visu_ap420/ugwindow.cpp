#include <stdio.h>
#include <stdint.h>
#include "ugwindow.h"
#include "ui_ugwindow.h"

ugwindow::ugwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::ugwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(messagePublish(const char *, const char *)),
            parent, SLOT(onMessagePublish(const char *, const char *)));
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

    if ((io->ugLight & ioState::ugLightBits::ugLightVorraum) == 0) {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: yellow");
    }

    if ((io->ugLight & ioState::ugLightBits::ugLightLager) == 0) {
        ui->pushButtonLightLager->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightLager->setStyleSheet("background-color: yellow");
    }

    if ((io->ugLight & ioState::ugLightBits::ugLightTechnik) == 0) {
        ui->pushButtonLightTechnik->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightTechnik->setStyleSheet("background-color: yellow");
    }

    if ((io->ugLight & ioState::ugLightBits::ugLightArbeit) == 0) {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: yellow");
    }

    if ((io->ugLight & ioState::ugLightBits::ugLightFitness) == 0) {
        ui->pushButtonLightFitness->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightFitness->setStyleSheet("background-color: yellow");
    }

    if ((io->ugLight & ioState::ugLightBits::ugLightStiege) == 0) {
        ui->pushButtonLightStiege->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightStiege->setStyleSheet("background-color: yellow");
    }
}

void ugwindow::on_pushButtonLightStiege_pressed() {

    ui->pushButtonLightStiege->setStyleSheet("background-color: grey");
    emit messagePublish("home/ug/stiege/licht/set", (io->ugLight & ioState::ugLightBits::ugLightStiege) ? "0" : "1");
}

void ugwindow::on_pushButtonLightVorraum_pressed() {

    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");
    emit messagePublish("home/ug/vorraum/licht/set", (io->ugLight & ioState::ugLightBits::ugLightVorraum) ? "0" : "1");
}

void ugwindow::on_pushButtonLightTechnik_pressed() {

    ui->pushButtonLightTechnik->setStyleSheet("background-color: grey");
    emit messagePublish("home/ug/technik/licht/set", (io->ugLight & ioState::ugLightBits::ugLightTechnik) ? "0" : "1");
}

void ugwindow::on_pushButtonLightLager_pressed() {

    ui->pushButtonLightLager->setStyleSheet("background-color: grey");
    emit messagePublish("home/ug/lager/licht/set", (io->ugLight & ioState::ugLightBits::ugLightLager) ? "0" : "1");
}

void ugwindow::on_pushButtonLightFitness_pressed() {

    ui->pushButtonLightFitness->setStyleSheet("background-color: grey");
    emit messagePublish("home/ug/fitness/licht/set", (io->ugLight & ioState::ugLightBits::ugLightFitness) ? "0" : "1");
}

void ugwindow::on_pushButtonLightArbeit_pressed() {

    ui->pushButtonLightArbeit->setStyleSheet("background-color: grey");
    emit messagePublish("home/ug/arbeit/licht/set", (io->ugLight & ioState::ugLightBits::ugLightArbeit) ? "0" : "1");
}

void ugwindow::on_pushButtonBack_clicked() {
    hide();
}
