#include <stdio.h>
#include "smartmeterwindow.h"
#include "ui_smartmeterwindow.h"

smartmeterwindow::smartmeterwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::smartmeterwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;

    connect(parent, SIGNAL(screenSaverActivated()), this, SLOT(onScreenSaverActivation()));
    connect(this, SIGNAL(messagePublish(const char *, const char *)),
            parent, SLOT(onMessagePublish(const char *, const char *)));
    connect(parent, SIGNAL(ioChanged(void)),
            this, SLOT(onIoStateChanged(void)));
}

smartmeterwindow::~smartmeterwindow() {
    delete ui;
}

void smartmeterwindow::onScreenSaverActivation(void) {
    if (isVisible) {
        hide();
    }
}

void smartmeterwindow::show(void) {

    emit messagePublish("home/smartmeter/enable-event/set", "01"); // variable as hex number

    isVisible = true;
    QString str;
    str = "Zählerstand A+: ";
    ui->label_aplus->setText(str);
    str = "Zählerstand A-: ";
    ui->label_aminus->setText(str);
    str = "Wirkleistung P+: ";
    ui->label_pplus->setText(str);
    str = "Wirkleistung P-: ";
    ui->label_pminus->setText(str);
    str = "Wirkleistung P: ";
    ui->label_p->setText(str);
    QDialog::show();
}

void smartmeterwindow::hide(void) {

    emit messagePublish("home/smartmeter/enable-event/set", "00"); // variable as hex number

    isVisible = false;
    QDialog::hide();
}

void smartmeterwindow::onIoStateChanged(void) {

    if (!isVisible) {
        return;
    }

    QString str;
    str = QString::number(io->sm.a_plus / 1000) +  " kWh";
    ui->label_aplus_val->setText(str);
    str = QString::number(io->sm.a_minus / 1000) +  " kWh";
    ui->label_aminus_val->setText(str);
    str = QString::number(io->sm.p_plus) + " W";
    ui->label_pplus_val->setText(str);
    str = QString::number(io->sm.p_minus) + " W";
    ui->label_pminus_val->setText(str);
    str = QString::number(io->sm.p_plus - io->sm.p_minus) + " W";
    ui->label_p_val->setText(str);
}

void smartmeterwindow::on_pushButtonBack_clicked() {
    hide();
}
