#include <stdio.h>
#include <stdint.h>
#include "ogwindow.h"
#include "ui_ogwindow.h"

ogwindow::ogwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::ogwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(messagePublish(const char *, const char *)),
            parent, SLOT(onMessagePublish(const char *, const char *)));
}

ogwindow::~ogwindow() {
    delete ui;
}

void ogwindow::show(void) {
    isVisible = true;
    onIoStateChanged();
    QDialog::show();
}

void ogwindow::hide(void) {
    isVisible = false;
    QDialog::hide();
}

void ogwindow::onIoStateChanged(void) {

    if (!isVisible) {
        return;
    }

    if ((io->ogLight & ioState::ogLightBits::ogLightAnna) == 0) {
        ui->pushButtonLightAnna->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightAnna->setStyleSheet("background-color: yellow");
    }

    if ((io->ogLight & ioState::ogLightBits::ogLightStiegePwr) == 0) {
        ui->pushButtonLightStiege->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightStiege->setStyleSheet("background-color: yellow");
    }

    if ((io->ogLight & ioState::ogLightBits::ogLightSeverin) == 0) {
        ui->pushButtonLightSeverin->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightSeverin->setStyleSheet("background-color: yellow");
    }

    if ((io->ogLight & ioState::ogLightBits::ogLightWC) == 0) {
        ui->pushButtonLightWC->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWC->setStyleSheet("background-color: yellow");
    }

    if ((io->ogLight & ioState::ogLightBits::ogLightBad) == 0) {
        ui->pushButtonLightBad->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightBad->setStyleSheet("background-color: yellow");
    }

    if ((io->ogLight & ioState::ogLightBits::ogLightBadSpiegel) == 0) {
        ui->pushButtonLightBadSpiegel->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightBadSpiegel->setStyleSheet("background-color: yellow");
    }

    if ((io->ogLight & ioState::ogLightBits::ogLightVorraum) == 0) {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: yellow");
    }

    if ((io->ogLight & ioState::ogLightBits::ogLightSchlaf) == 0) {
        ui->pushButtonLightSchlaf->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightSchlaf->setStyleSheet("background-color: yellow");
    }

    if ((io->ogLight & ioState::ogLightBits::ogLightSchrank) == 0) {
        ui->pushButtonLightSchrank->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightSchrank->setStyleSheet("background-color: yellow");
    }
}

void ogwindow::on_pushButtonLightAnna_pressed() {

    ui->pushButtonLightAnna->setStyleSheet("background-color: grey");
    emit messagePublish("home/og/anna/licht/set", (io->ogLight & ioState::ogLightBits::ogLightAnna) ? "0" : "1");
}

void ogwindow::on_pushButtonLightSeverin_pressed() {

    ui->pushButtonLightSeverin->setStyleSheet("background-color: grey");
    emit messagePublish("home/og/severin/licht/set", (io->ogLight & ioState::ogLightBits::ogLightSeverin) ? "0" : "1");
}

void ogwindow::on_pushButtonLightWC_pressed() {

    ui->pushButtonLightWC->setStyleSheet("background-color: grey");
    emit messagePublish("home/og/wc/licht/set", (io->ogLight & ioState::ogLightBits::ogLightWC) ? "0" : "1");
}

void ogwindow::on_pushButtonLightBad_pressed() {

    ui->pushButtonLightBad->setStyleSheet("background-color: grey");
    emit messagePublish("home/og/bad/licht/set", (io->ogLight & ioState::ogLightBits::ogLightBad) ? "0" : "1");
}

void ogwindow::on_pushButtonLightBadSpiegel_pressed() {

    ui->pushButtonLightBadSpiegel->setStyleSheet("background-color: grey");
    emit messagePublish("home/og/bad/spiegel/licht/set", (io->ogLight & ioState::ogLightBits::ogLightBadSpiegel) ? "0" : "1");
}

void ogwindow::on_pushButtonLightVorraum_pressed() {

    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");
    emit messagePublish("home/og/vorraum/licht/set", (io->ogLight & ioState::ogLightBits::ogLightVorraum) ? "0" : "1");
}

void ogwindow::on_pushButtonLightStiege_pressed() {

    ui->pushButtonLightStiege->setStyleSheet("background-color: grey");
    emit messagePublish("home/og/stiege/netzteil/set", (io->ogLight & ioState::ogLightBits::ogLightStiegePwr) ? "0" : "1");
    emit messagePublish("home/og/stiege/licht1/set", (io->ogLight & ioState::ogLightBits::ogLightStiegePwr) ? "0" : "1");
    emit messagePublish("home/og/stiege/licht2/set", (io->ogLight & ioState::ogLightBits::ogLightStiegePwr) ? "0" : "1");
    emit messagePublish("home/og/stiege/licht3/set", (io->ogLight & ioState::ogLightBits::ogLightStiegePwr) ? "0" : "1");
    emit messagePublish("home/og/stiege/licht4/set", (io->ogLight & ioState::ogLightBits::ogLightStiegePwr) ? "0" : "1");
    emit messagePublish("home/og/stiege/licht5/set", (io->ogLight & ioState::ogLightBits::ogLightStiegePwr) ? "0" : "1");
    emit messagePublish("home/og/stiege/licht6/set", (io->ogLight & ioState::ogLightBits::ogLightStiegePwr) ? "0" : "1");
}

void ogwindow::on_pushButtonLightSchlaf_pressed() {

    ui->pushButtonLightSchlaf->setStyleSheet("background-color: grey");
    emit messagePublish("home/og/schlaf/licht/set", (io->ogLight & ioState::ogLightBits::ogLightSchlaf) ? "0" : "1");
}

void ogwindow::on_pushButtonLightSchrank_pressed() {

    ui->pushButtonLightSchrank->setStyleSheet("background-color: grey");
    emit messagePublish("home/og/schrank/licht/set", (io->ogLight & ioState::ogLightBits::ogLightSchrank) ? "0" : "1");

}

void ogwindow::on_pushButtonBack_clicked() {
    hide();
}
