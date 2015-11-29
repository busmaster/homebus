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
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(serviceCmd(const char *)), parent, SLOT(onSendServiceCmd(const char *)));
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

int ogwindow::do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen) {
    size_t i;
    int len;

    len = snprintf(pCmd, cmdLen, "-a %d -setvaldo31_do", do31Addr);

    for (i = 0; i < stateLen; i++) {
        len += snprintf(pCmd + len, cmdLen - len, " %d", *(pDoState + i));
    }
    return len;
}

void ogwindow::on_pushButtonLightAnna_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ogState.detail.lightAnna == 0) {
        doState[27] = 3; // on
    } else {
        doState[27] = 2; // off
    }
    do31Cmd(241, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightAnna->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ogwindow::on_pushButtonLightSeverin_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ogState.detail.lightSeverin == 0) {
        doState[28] = 3; // on
    } else {
        doState[28] = 2; // off
    }
    do31Cmd(241, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightSeverin->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ogwindow::on_pushButtonLightWC_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ogState.detail.lightWC == 0) {
        doState[29] = 3; // on
    } else {
        doState[29] = 2; // off
    }
    do31Cmd(241, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightWC->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ogwindow::on_pushButtonLightBad_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ogState.detail.lightBad == 0) {
        doState[12] = 3; // on
    } else {
        doState[12] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightBad->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ogwindow::on_pushButtonLightBadSpiegel_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ogState.detail.lightBadSpiegel == 0) {
        doState[10] = 3; // on
    } else {
        doState[10] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightBadSpiegel->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ogwindow::on_pushButtonLightVorraum_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ogState.detail.lightVorraum == 0) {
        doState[11] = 3; // on
    } else {
        doState[11] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ogwindow::on_pushButtonLightStiege_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ogState.detail.lightStiegePwr == 0) {
        doState[0] = 3; // on
        doState[1] = 3; // on
        doState[2] = 3; // on
        doState[3] = 3; // on
        doState[4] = 3; // on
        doState[5] = 3; // on
        doState[6] = 3; // on
    } else {
        doState[0] = 2; // off
        doState[1] = 2; // off
        doState[2] = 2; // off
        doState[3] = 2; // off
        doState[4] = 2; // off
        doState[5] = 2; // off
        doState[6] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightStiege->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ogwindow::on_pushButtonLightSchlaf_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ogState.detail.lightSchlaf == 0) {
        doState[8] = 3; // on
    } else {
        doState[8] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightSchlaf->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void ogwindow::on_pushButtonLightSchrank_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->ogState.detail.lightSchrank == 0) {
        doState[7] = 3; // on
    } else {
        doState[7] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightSchrank->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}
