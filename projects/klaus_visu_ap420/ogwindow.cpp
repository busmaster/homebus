#include "ogwindow.h"
#include "ui_ogwindow.h"

ogwindow::ogwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::ogwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    modulservice = new QProcess;
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

void ogwindow::on_pushButtonLightAnna_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 241 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ogState.detail.lightAnna == 0) {
        doState[27 * 2] = '3'; // on
    } else {
        doState[27 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightAnna->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ogwindow::on_pushButtonLightSeverin_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 241 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ogState.detail.lightSeverin == 0) {
        doState[28 * 2] = '3'; // on
    } else {
        doState[28 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightSeverin->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ogwindow::on_pushButtonLightWC_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 241 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ogState.detail.lightWC == 0) {
        doState[29 * 2] = '3'; // on
    } else {
        doState[29 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightWC->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ogwindow::on_pushButtonLightBad_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ogState.detail.lightBad == 0) {
        doState[12 * 2] = '3'; // on
    } else {
        doState[12 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightBad->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ogwindow::on_pushButtonLightBadSpiegel_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ogState.detail.lightBadSpiegel == 0) {
        doState[10 * 2] = '3'; // on
    } else {
        doState[10 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightBadSpiegel->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ogwindow::on_pushButtonLightVorraum_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ogState.detail.lightVorraum == 0) {
        doState[11 * 2] = '3'; // on
    } else {
        doState[11 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightVorraum->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ogwindow::on_pushButtonLightStiege_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ogState.detail.lightStiegePwr == 0) {
        doState[0 * 2] = '3'; // on
        doState[1 * 2] = '3'; // on
        doState[2 * 2] = '3'; // on
        doState[3 * 2] = '3'; // on
        doState[4 * 2] = '3'; // on
        doState[5 * 2] = '3'; // on
        doState[6 * 2] = '3'; // on
    } else {
        doState[0 * 2] = '2'; // off
        doState[1 * 2] = '2'; // off
        doState[2 * 2] = '2'; // off
        doState[3 * 2] = '2'; // off
        doState[4 * 2] = '2'; // off
        doState[5 * 2] = '2'; // off
        doState[6 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightStiege->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ogwindow::on_pushButtonLightSchlaf_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ogState.detail.lightSchlaf == 0) {
        doState[8 * 2] = '3'; // on
    } else {
        doState[8 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightSchlaf->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}

void ogwindow::on_pushButtonLightSchrank_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->ogState.detail.lightSchrank == 0) {
        doState[7 * 2] = '3'; // on
    } else {
        doState[7 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLightSchrank->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}
