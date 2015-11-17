#include <stdio.h>
#include <stdint.h>
#include "egwindow.h"
#include "ui_egwindow.h"
#include "moduleservice.h"

egwindow::egwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::egwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    connect(this, SIGNAL(serviceCmd(const char *)), parent, SLOT(onSendServiceCmd(const char *)));
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

    if (io->egState.detail.lightGang == 0) {
        ui->pushButtonLightGang->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightGang->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightEss == 0) {
        ui->pushButtonLightEss->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightEss->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightWohn == 0) {
        ui->pushButtonLightWohn->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWohn->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightWC == 0) {
        ui->pushButtonLightWC->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWC->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightVorraum == 0) {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightArbeit == 0) {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightKueche == 0) {
        ui->pushButtonLightKueche->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightKueche->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightKuecheWand == 0) {
        ui->pushButtonLightKuecheWand->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightKuecheWand->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightSpeis == 0) {
        ui->pushButtonLightSpeis->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightSpeis->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightTerrasse == 0) {
        ui->pushButtonLightTerrasse->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightTerrasse->setStyleSheet("background-color: yellow");
    }

    if (io->egState.detail.lightWohnLese == 0) {
        ui->pushButtonLightWohnLese->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightWohnLese->setStyleSheet("background-color: yellow");
    }

}

int egwindow::do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen) {
    size_t i;
    int len;

    len = snprintf(pCmd, cmdLen, "-a %d -setvaldo31_do", do31Addr);

    for (i = 0; i < stateLen; i++) {
        len += snprintf(pCmd + len, cmdLen - len, " %d", *(pDoState + i));
    }
    return len;
}

void egwindow::on_pushButtonLightGang_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightGang == 0) {
        doState[25] = 3; // on
    } else {
        doState[25] = 2; // off
    }
    do31Cmd(241, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightGang->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void egwindow::on_pushButtonLightWohn_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightWohn == 0) {
        doState[13] = 3; // on
    } else {
        doState[13] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightWohn->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void egwindow::on_pushButtonLightKueche_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightKueche == 0) {
        doState[19] = 3; // on
    } else {
        doState[19] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightKueche->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void egwindow::on_pushButtonLightEss_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightEss == 0) {
        doState[15] = 3; // on
    } else {
        doState[15] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightEss->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void egwindow::on_pushButtonLightVorraum_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightVorraum == 0) {
        doState[16] = 3; // on
    } else {
        doState[16] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void egwindow::on_pushButtonLightKuecheWand_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightKuecheWand == 0) {
        doState[30] = 3; // on
    } else {
        doState[30] = 2; // off
    }
    do31Cmd(241, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightKuecheWand->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void egwindow::on_pushButtonLightArbeit_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightArbeit == 0) {
        doState[26] = 3; // on
    } else {
        doState[26] = 2; // off
    }
    do31Cmd(241, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightArbeit->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void egwindow::on_pushButtonLightSpeis_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightSpeis == 0) {
        doState[24] = 3; // on
    } else {
        doState[24] = 2; // off
    }
    do31Cmd(241, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightSpeis->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void egwindow::on_pushButtonLightWC_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightWC == 0) {
        doState[17] = 3; // on
    } else {
        doState[17] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightWC->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}

void egwindow::on_pushButtonLightTerrasse_pressed() {
    char    command[250];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightTerrasse == 0) {
        doState[20] = 3; // on
    } else {
        doState[20] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightTerrasse->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}



void egwindow::on_pushButtonLightWohnLese_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->egState.detail.lightWohnLese == 0) {
        doState[14] = 3; // on
    } else {
        doState[14] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonLightWohnLese->setStyleSheet("background-color: grey");

    emit serviceCmd(command);
}
