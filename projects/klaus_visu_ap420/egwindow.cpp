#include "egwindow.h"
#include "ui_egwindow.h"

egwindow::egwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::egwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    modulservice = new QProcess;
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
}

void egwindow::on_pushButtonLightGang_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightGang == 0) {
        doState[14 * 2] = '3'; // on
    } else {
        doState[14 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightGang->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void egwindow::on_pushButtonLightWohn_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightWohn == 0) {
        doState[13 * 2] = '3'; // on
    } else {
        doState[13 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightWohn->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void egwindow::on_pushButtonLightKueche_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightKueche == 0) {
        doState[19 * 2] = '3'; // on
    } else {
        doState[19 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightKueche->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void egwindow::on_pushButtonLightEss_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightEss == 0) {
        doState[15 * 2] = '3'; // on
    } else {
        doState[15 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightEss->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void egwindow::on_pushButtonLightVorraum_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightVorraum == 0) {
        doState[16 * 2] = '3'; // on
    } else {
        doState[16 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void egwindow::on_pushButtonLightKuecheWand_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 241 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightKuecheWand == 0) {
        doState[30 * 2] = '3'; // on
    } else {
        doState[30 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightKuecheWand->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void egwindow::on_pushButtonLightArbeit_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 241 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightArbeit == 0) {
        doState[26 * 2] = '3'; // on
    } else {
        doState[26 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightArbeit->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void egwindow::on_pushButtonLightSpeis_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 241 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightSpeis == 0) {
        doState[24 * 2] = '3'; // on
    } else {
        doState[24 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightSpeis->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void egwindow::on_pushButtonLightWC_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightWC == 0) {
        doState[17 * 2] = '3'; // on
    } else {
        doState[17 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightWC->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void egwindow::on_pushButtonLightTerrasse_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->egState.detail.lightTerrasse == 0) {
        doState[20 * 2] = '3'; // on
    } else {
        doState[20 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightTerrasse->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}
