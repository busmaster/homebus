#include <stdio.h>
#include <stdint.h>
#include "kuechewindow.h"
#include "ui_kuechewindow.h"

kuechewindow::kuechewindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::kuechewindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(messagePublish(const char *, const char *)),
            parent, SLOT(onMessagePublish(const char *, const char *)));
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

    if ((io->kueche & ioState::kuecheLightBits::kuecheLight) == 0) {
        ui->pushButtonLight->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLight->setStyleSheet("background-color: yellow");
    }

    if ((io->kueche & ioState::kuecheLightBits::kuecheLightWand) == 0) {
        ui->pushButtonLightWand->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWand->setStyleSheet("background-color: yellow");
    }

    if ((io->kueche & ioState::kuecheLightBits::kuecheLightDunstabzug) == 0) {
        ui->pushButtonLightDunstabzug->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightDunstabzug->setStyleSheet("background-color: yellow");
    }

    if ((io->kueche & ioState::kuecheLightBits::kuecheLightAbwasch) == 0) {
        ui->pushButtonLightAbwasch->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightAbwasch->setStyleSheet("background-color: yellow");
    }

    if ((io->kueche & ioState::kuecheLightBits::kuecheLightGeschirrspueler) == 0) {
        ui->pushButtonLightGeschirrspueler->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightGeschirrspueler->setStyleSheet("background-color: yellow");
    }

    if ((io->kueche & ioState::kuecheLightBits::kuecheLightKaffee) == 0) {
        ui->pushButtonLightKaffee->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightKaffee->setStyleSheet("background-color: yellow");
    }

    if ((io->kueche & ioState::kuecheLightBits::kuecheLightSpeis) == 0) {
        ui->pushButtonLightSpeis->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightSpeis->setStyleSheet("background-color: yellow");
    }
}

void kuechewindow::on_pushButtonLight_pressed() {

    ui->pushButtonLight->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/kueche/licht/set", (io->kueche & ioState::kuecheLightBits::kuecheLight) ? "0" : "1");
}

void kuechewindow::on_pushButtonLightWand_pressed() {

    ui->pushButtonLightWand->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/kueche/wand/licht/set", (io->kueche & ioState::kuecheLightBits::kuecheLightWand) ? "0" : "1");
}

void kuechewindow::on_pushButtonLightSpeis_pressed() {

    ui->pushButtonLightSpeis->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/speis/licht/set", (io->kueche & ioState::kuecheLightBits::kuecheLightSpeis) ? "0" : "1");
}

void kuechewindow::on_pushButtonLightGeschirrspueler_pressed() {

    ui->pushButtonLightGeschirrspueler->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/kueche/geschirrspueler/licht/set", (io->kueche & ioState::kuecheLightBits::kuecheLightGeschirrspueler) ? "0" : "1");
}

void kuechewindow::on_pushButtonLightAbwasch_pressed() {

    ui->pushButtonLightAbwasch->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/kueche/abwasch/licht/set", (io->kueche & ioState::kuecheLightBits::kuecheLightAbwasch) ? "0" : "1");
}

void kuechewindow::on_pushButtonLightKaffee_pressed() {

    ui->pushButtonLightKaffee->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/kueche/kaffeemaschine/licht/set", (io->kueche & ioState::kuecheLightBits::kuecheLightKaffee) ? "0" : "1");
}

void kuechewindow::on_pushButtonLightDunstabzug_pressed() {

    ui->pushButtonLightDunstabzug->setStyleSheet("background-color: grey");
    emit messagePublish("home/eg/kueche/dunstabzug/licht/set", (io->kueche & ioState::kuecheLightBits::kuecheLightDunstabzug) ? "0" : "1");

}

void kuechewindow::on_verticalSlider_valueChanged(int value) {

    /*
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
    */
}

void kuechewindow::on_pushButtonBack_clicked() {
    hide();
}
