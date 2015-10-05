#include "ugwindow.h"
#include "ui_ugwindow.h"

ugwindow::ugwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::ugwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    modulservice = new QProcess;
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

    if (io->ugState.detail.lightVorraum == 0) {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightVorraum->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightLager == 0) {
        ui->pushButtonLightLager->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightLager->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightTechnik == 0) {
        ui->pushButtonLightTechnik->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightTechnik->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightArbeit == 0) {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightArbeit->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightFitness == 0) {
        ui->pushButtonLightFitness->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightFitness->setStyleSheet("background-color: yellow");
    }

    if (io->ugState.detail.lightStiege == 0) {
        ui->pushButtonLightStiege->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLightStiege->setStyleSheet("background-color: yellow");
    }

}

void ugwindow::on_pushButtonLightStiege_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ugState.detail.lightStiege == 0) {
        doState[22 * 2] = '3'; // on
    } else {
        doState[22 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightStiege->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ugwindow::on_pushButtonLightVorraum_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ugState.detail.lightVorraum == 0) {
        doState[25 * 2] = '3'; // on
    } else {
        doState[25 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ugwindow::on_pushButtonLightTechnik_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ugState.detail.lightTechnik == 0) {
        doState[26 * 2] = '3'; // on
    } else {
        doState[26 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightTechnik->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ugwindow::on_pushButtonLightLager_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ugState.detail.lightLager == 0) {
        doState[21 * 2] = '3'; // on
    } else {
        doState[21 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightLager->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ugwindow::on_pushButtonLightFitness_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ugState.detail.lightFitness == 0) {
        doState[24 * 2] = '3'; // on
    } else {
        doState[24 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightFitness->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ugwindow::on_pushButtonLightArbeit_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ugState.detail.lightArbeit == 0) {
        doState[23 * 2] = '3'; // on
    } else {
        doState[23 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightArbeit->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}
