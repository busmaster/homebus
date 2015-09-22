#include "garagewindow.h"
#include "ui_garagewindow.h"

garagewindow::garagewindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::garagewindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    modulservice = new QProcess;
}

garagewindow::~garagewindow() {
    delete ui;
}

void garagewindow::show(void) {
    isVisible = true;
    onIoStateChanged();
    QDialog::show();
}

void garagewindow::hide(void) {
    isVisible = false;
    QDialog::hide();
}

void garagewindow::onIoStateChanged(void) {

    if (!isVisible) {
        return;
    }

    if (io->garageState.detail.light == 0) {
        ui->pushButtonLight->setStyleSheet("background-color: green");
    } else {
        ui->pushButtonLight->setStyleSheet("background-color: yellow");
    }
}

void garagewindow::on_pushButtonLight_pressed() {
    char command[250];
    char doState[100];
    int i;
    strcpy(command, "/root/modulservice -c /dev/hausbus1 -a 240 -setvaldo31_do ");
    for (i = 0; i < 31; i++) {
        doState[i * 2] = '0';
        doState[i * 2 + 1] = ' ';
    }
    doState[i * 2] = '\0';
    if (io->garageState.detail.light == 0) {
        doState[9 * 2] = '3'; // on
    } else {
        doState[9 * 2] = '2'; // off
    }
    strcat(command, doState);
    ui->pushButtonLight->setStyleSheet("background-color: grey");
    modulservice->start((QString)command);
}
