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

void ogwindow::on_pushButtonLightAnna_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 241;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ogState.detail.lightAnna == 0) {
        command.data.setvaldo31_do.setval[27] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[27] = 2; // off
    }

    ui->pushButtonLightAnna->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightAnna;
    currentButtonState = (io->ogState.detail.lightAnna == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ogwindow::on_pushButtonLightSeverin_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 241;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ogState.detail.lightSeverin == 0) {
        command.data.setvaldo31_do.setval[28] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[28] = 2; // off
    }

    ui->pushButtonLightSeverin->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightSeverin;
    currentButtonState = (io->ogState.detail.lightSeverin == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ogwindow::on_pushButtonLightWC_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 241;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ogState.detail.lightWC == 0) {
        command.data.setvaldo31_do.setval[29] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[29] = 2; // off
    }

    ui->pushButtonLightWC->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightWC;
    currentButtonState = (io->ogState.detail.lightWC == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ogwindow::on_pushButtonLightBad_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ogState.detail.lightBad == 0) {
        command.data.setvaldo31_do.setval[12] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[12] = 2; // off
    }

    ui->pushButtonLightBad->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightBad;
    currentButtonState = (io->ogState.detail.lightBad == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ogwindow::on_pushButtonLightBadSpiegel_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ogState.detail.lightBadSpiegel == 0) {
        command.data.setvaldo31_do.setval[10] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[10] = 2; // off
    }

    ui->pushButtonLightBadSpiegel->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightBadSpiegel;
    currentButtonState = (io->ogState.detail.lightBadSpiegel == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ogwindow::on_pushButtonLightVorraum_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ogState.detail.lightVorraum == 0) {
        command.data.setvaldo31_do.setval[11] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[11] = 2; // off
    }

    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightVorraum;
    currentButtonState = (io->ogState.detail.lightVorraum == 0) ? false : true;

    emit serviceCmd(&command, this);
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

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ogState.detail.lightSchlaf == 0) {
        command.data.setvaldo31_do.setval[8] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[8] = 2; // off
    }

    ui->pushButtonLightSchlaf->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightSchlaf;
    currentButtonState = (io->ogState.detail.lightSchlaf == 0) ? false : true;

    emit serviceCmd(&command, this);
}

void ogwindow::on_pushButtonLightSchrank_pressed() {

    struct moduleservice::cmd command;

    command.type = moduleservice::eSetvaldo31_do;
    command.destAddr = 240;

    memset(&command.data, 0, sizeof(command.data));
    if (io->ogState.detail.lightSchrank == 0) {
        command.data.setvaldo31_do.setval[7] = 3; // on
    } else {
        command.data.setvaldo31_do.setval[7] = 2; // off
    }

    ui->pushButtonLightSchrank->setStyleSheet("background-color: grey");
    currentButton = ui->pushButtonLightSchrank;
    currentButtonState = (io->ogState.detail.lightSchrank == 0) ? false : true;

    emit serviceCmd(&command, this);
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
