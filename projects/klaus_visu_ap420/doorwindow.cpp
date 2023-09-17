#include <stdio.h>
#include <stdint.h>
#include "doorwindow.h"
#include "ui_doorwindow.h"

doorwindow::doorwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::doorwindow) {
    ui->setupUi(this);
    ui->pushButtonLock->setStyleSheet("background-color: green; color: black");
    ui->pushButtonUnlock->setStyleSheet("background-color: red; color: black");
    io = state;
    isVisible = false;
    connect(parent, SIGNAL(doorChanged()),
            this, SLOT(onDoorChanged()));
    connect(this, SIGNAL(messagePublish(const char*,const char*)),
            parent, SLOT(onMessagePublish(const char*,const char*)));

    openButtonTimer = new QTimer(this);
    openButtonTimer->setSingleShot(true);
    connect(openButtonTimer, SIGNAL(timeout()), this, SLOT(openButtonTimerEvent()));

    lockButtonTimer = new QTimer(this);
    lockButtonTimer->setSingleShot(true);
    connect(lockButtonTimer, SIGNAL(timeout()), this, SLOT(lockButtonTimerEvent()));

    refreshButtonTimer = new QTimer(this);
    refreshButtonTimer->setSingleShot(true);
    connect(refreshButtonTimer, SIGNAL(timeout()), this, SLOT(refreshButtonTimerEvent()));

}

doorwindow::~doorwindow() {
    delete ui;
}

void doorwindow::show(void) {
    isVisible = true;
    onDoorChanged();
    ui->pushButtonRefresh->setStyleSheet("background-color: grey; color:black");
    ui->pushButtonRefresh->setText("Aktualisieren");

    ui->groupBoxLock->setStyleSheet("background-color: grey");
    ui->groupBoxUnlock->setStyleSheet("background-color: grey");
    ui->pushButtonOpen->setStyleSheet("background-color: red; color:black");

    on_pushButtonRefresh_clicked();

    QDialog::show();
}

void doorwindow::hide(void) {
    isVisible = false;
    QDialog::hide();
}

void doorwindow::openButtonTimerEvent() {

    emit messagePublish("home/door/lock/set", "");
}

void doorwindow::lockButtonTimerEvent() {

    emit messagePublish("home/door/lock/set", "");
}

void doorwindow::refreshButtonTimerEvent() {

    ui->pushButtonRefresh->setStyleSheet("background-color: grey; color:black");
    ui->pushButtonRefresh->setDisabled(false);
}


void doorwindow::onDoorChanged(void) {

    if (!isVisible) {
        return;
    }

    if (!ui->pushButtonRefresh->isEnabled()) {
        ui->pushButtonRefresh->setStyleSheet("background-color: grey; color:black");
        ui->pushButtonRefresh->setDisabled(false);
        refreshButtonTimer->stop();
    }

    if (!ui->pushButtonLock->isEnabled()) {
        ui->pushButtonLock->setStyleSheet("background-color: green; color:black");
        ui->pushButtonLock->setDisabled(false);
        lockButtonTimer->stop();
    }

    if (!ui->pushButtonOpen->isEnabled()) {
        ui->pushButtonOpen->setStyleSheet("background-color: red; color:black");
        ui->pushButtonOpen->setDisabled(false);
        openButtonTimer->stop();
    }

    switch (io->door.lockstate) {
    case ioState::doorState::locked:
        ui->groupBoxLock->setStyleSheet("background-color: yellow");
        ui->groupBoxUnlock->setStyleSheet("background-color: grey");
        break;
    case ioState::doorState::unlocked:
        ui->groupBoxLock->setStyleSheet("background-color: grey");
        ui->groupBoxUnlock->setStyleSheet("background-color: yellow");
        break;
    case ioState::doorState::again:
        break;
    case ioState::doorState::internal:
        break;
    case ioState::doorState::invalid1:
        break;
    case ioState::doorState::invalid2:
        break;
    case ioState::doorState::noresp:
        break;
    case ioState::doorState::noconnection:
        break;
    default:
        break;
    }
}

void doorwindow::on_pushButtonRefresh_clicked() {

    ui->pushButtonRefresh->setStyleSheet("background-color: yellow; color:black");
    ui->groupBoxLock->setStyleSheet("background-color: grey");
    ui->groupBoxUnlock->setStyleSheet("background-color: grey");
    ui->pushButtonRefresh->setDisabled(true);

    refreshButtonTimer->start(6000);

    emit messagePublish("home/door/lock/set", "");
}

void doorwindow::on_pushButtonLock_pressed() {

    ui->pushButtonLock->setStyleSheet("background-color: yellow; color:black");
    ui->groupBoxLock->setStyleSheet("background-color: grey");
    ui->groupBoxUnlock->setStyleSheet("background-color: grey");
    ui->pushButtonLock->setDisabled(true);

    lockButtonTimer->start(6000);

    emit messagePublish("home/door/lock/set", "lock");
}

void doorwindow::on_pushButtonOpen_pressed() {

    ui->groupBoxLock->setStyleSheet("background-color: grey");
    ui->groupBoxUnlock->setStyleSheet("background-color: grey");
    ui->pushButtonOpen->setStyleSheet("background-color: yellow; color:black");
    ui->pushButtonOpen->setDisabled(true);

    openButtonTimer->start(8000);

    emit messagePublish("home/door/lock/set", "unlock");
}

void doorwindow::on_pushButtonBack_clicked() {
    hide();
}
