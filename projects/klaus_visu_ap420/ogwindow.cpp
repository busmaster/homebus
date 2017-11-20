#include <stdio.h>
#include <stdint.h>
#include "ogwindow.h"
#include "ui_ogwindow.h"
#include "moduleservice.h"

ogwindow::ogwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::ogwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(serviceCmd(const moduleservice::cmd *, QDialog *)),
            parent, SLOT(onSendServiceCmd(const struct moduleservice::cmd *, QDialog *)));
    connect(parent, SIGNAL(cmdConf(const struct moduleservice::result *, QDialog *)),
            this, SLOT(onCmdConf(const struct moduleservice::result *, QDialog *)));}

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

    if (io->ogState.detail.lightAnna == 0) {
        ui->pushButtonLightAnna->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightAnna->setStyleSheet("background-color: yellow");
    }

    if (io->ogState.detail.lightStiegePwr == 0) {
        ui->pushButtonLightStiege->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightStiege->setStyleSheet("background-color: yellow");
    }

    if (io->ogState.detail.lightSeverin == 0) {
        ui->pushButtonLightSeverin->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightSeverin->setStyleSheet("background-color: yellow");
    }

    if (io->ogState.detail.lightWC == 0) {
        ui->pushButtonLightWC->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWC->setStyleSheet("background-color: yellow");
    }

    if (io->ogState.detail.lightBad == 0) {
        ui->pushButtonLightBad->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightBad->setStyleSheet("background-color: yellow");
    }

    if (io->ogState.detail.lightBadSpiegel == 0) {
        ui->pushButtonLightBadSpiegel->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightBadSpiegel->setStyleSheet("background-color: yellow");
    }

    if (io->ogState.detail.lightVorraum == 0) {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: yellow");
    }

    if (io->ogState.detail.lightSchlaf == 0) {
        ui->pushButtonLightSchlaf->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightSchlaf->setStyleSheet("background-color: yellow");
    }

    if (io->ogState.detail.lightSchrank == 0) {
        ui->pushButtonLightSchrank->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightSchrank->setStyleSheet("background-color: yellow");
    }
}

void ogwindow::sendDo31Cmd(quint8 destAddr, quint8 doNr, QPushButton *button, bool currState) {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = destAddr;

    memset(&command.data, 0, sizeof(command.data));
    currentButtonState = currState;
    if (currState) {
        command.data.setvaldo31_do.setval[doNr] = 2; // off
    } else {
        command.data.setvaldo31_do.setval[doNr] = 3; // on
    }

    button->setStyleSheet("background-color: grey");
    currentButton = button;

    emit serviceCmd(&command, this);
}

void ogwindow::on_pushButtonLightAnna_pressed() {

    sendDo31Cmd(241, 27, ui->pushButtonLightAnna, io->ogState.detail.lightAnna != 0);
}

void ogwindow::on_pushButtonLightSeverin_pressed() {

    sendDo31Cmd(241, 28, ui->pushButtonLightSeverin, io->ogState.detail.lightSeverin != 0);
}

void ogwindow::on_pushButtonLightWC_pressed() {

    sendDo31Cmd(241, 29, ui->pushButtonLightWC, io->ogState.detail.lightWC != 0);
}

void ogwindow::on_pushButtonLightBad_pressed() {

    sendDo31Cmd(240, 12, ui->pushButtonLightBad, io->ogState.detail.lightBad != 0);
}

void ogwindow::on_pushButtonLightBadSpiegel_pressed() {

    sendDo31Cmd(240, 10, ui->pushButtonLightBadSpiegel, io->ogState.detail.lightBadSpiegel != 0);
}

void ogwindow::on_pushButtonLightVorraum_pressed() {

    sendDo31Cmd(240, 11, ui->pushButtonLightVorraum, io->ogState.detail.lightVorraum != 0);
}

void ogwindow::on_pushButtonLightStiege_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ogState.detail.lightStiegePwr == 0) {
        command.data.setvaldo31_do.setval[0] = 3; // on
        command.data.setvaldo31_do.setval[1] = 3; // on
        command.data.setvaldo31_do.setval[2] = 3; // on
        command.data.setvaldo31_do.setval[3] = 3; // on
        command.data.setvaldo31_do.setval[4] = 3; // on
        command.data.setvaldo31_do.setval[5] = 3; // on
        command.data.setvaldo31_do.setval[6] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[0] = 2; // off
        command.data.setvaldo31_do.setval[1] = 2; // off
        command.data.setvaldo31_do.setval[2] = 2; // off
        command.data.setvaldo31_do.setval[3] = 2; // off
        command.data.setvaldo31_do.setval[4] = 2; // off
        command.data.setvaldo31_do.setval[5] = 2; // off
        command.data.setvaldo31_do.setval[6] = 2; // off
    }

    ui->pushButtonLightStiege->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightStiege;
    currentButtonState = (io->ogState.detail.lightStiegePwr == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ogwindow::on_pushButtonLightSchlaf_pressed() {

    sendDo31Cmd(240, 8, ui->pushButtonLightSchlaf, io->ogState.detail.lightSchlaf != 0);
}

void ogwindow::on_pushButtonLightSchrank_pressed() {

    sendDo31Cmd(240, 7, ui->pushButtonLightSchrank, io->ogState.detail.lightSchrank != 0);
}

void ogwindow::onCmdConf(const struct moduleservice::result *res, QDialog *dialog) {

    if ((dialog == this) && (res->data.state == moduleservice::eCmdOk)) {
        if (currentButtonState) {
            currentButton->setStyleSheet("background-color: green");
        } else {
            currentButton->setStyleSheet("background-color: yellow");
        }
//        printf("ogwindow cmdconf %d\n", res->data.state);
    }
}

void ogwindow::on_pushButtonBack_clicked() {
    hide();
}
