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
    connect(this, SIGNAL(messagePublish(const char*,const char*)),
            parent, SLOT(onMessagePublish(const char*,const char*)));
    connect(parent, SIGNAL(ioChanged()),
            this, SLOT(onIoStateChanged()));
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
    str = "Zählerstand A+";
    ui->label_aplus->setText(str);
    str = "Zählerstand A\u2212";
    ui->label_aminus->setText(str);
    str = "Wirkleistung P+";
    ui->label_pplus->setText(str);
    str = "Wirkleistung P\u2212";
    ui->label_pminus->setText(str);
    str = "Wirkleistung P";
    ui->label_p->setText(str);
    str = "Bezug Tag";
    ui->label_aplus_day->setText(str);
    str = "Einspeisung Tag";
    ui->label_aminus_day->setText(str);
    str = "PV Tag";
    ui->label_solar_day->setText(str);

    QDialog::show();
}

void smartmeterwindow::hide(void) {

    emit messagePublish("home/smartmeter/enable-event/set", "00"); // variable as hex number

    isVisible = false;
    QDialog::hide();
}

void smartmeterwindow::onIoStateChanged(void) {

    uint32_t counter;

    if (!isVisible) {
        return;
    }

    ui->label_aplus_val->setText(QString::asprintf("%d.%03d kWh", io->sm.a_plus / 1000, io->sm.a_plus % 1000));
    ui->label_aminus_val->setText(QString::asprintf("%d.%03d kWh", io->sm.a_minus / 1000, io->sm.a_minus % 1000));
    ui->label_pplus_val->setText(QString::number(io->sm.p_plus) + " W");
    ui->label_pminus_val->setText(QString::number(io->sm.p_minus) + " W");
    if (io->sm.p_plus > io->sm.p_minus) {
        ui->label_p_val->setStyleSheet("background-color: rgba(255, 255, 255, 0);color: red;font: 24pt \"Sans\"");
        ui->label_p_val->setText(QString::number(io->sm.p_plus - io->sm.p_minus) + " W");
    } else {
        ui->label_p_val->setStyleSheet("background-color: rgba(255, 255, 255, 0);color: green;font: 24pt \"Sans\"");
        ui->label_p_val->setText(QString::number(io->sm.p_minus - io->sm.p_plus) + " W");
    }

    counter = io->sm.a_plus - io->sm.a_plus_midnight;
    ui->label_aplus_day_val->setText(QString::asprintf("%d.%03d kWh", counter / 1000, counter % 1000));
    counter = io->sm.a_minus - io->sm.a_minus_midnight;
    ui->label_aminus_day_val->setText(QString::asprintf("%d.%03d kWh", counter / 1000, counter % 1000));

    counter = io->solar.oben_ost + io->solar.oben_mitte + io->solar.oben_west +
              io->solar.unten_ost + io->solar.unten_mitte + io->solar.unten_west;
    ui->label_solar_day_val->setText(QString::asprintf("%d.%03d kWh", counter / 1000, counter % 1000));

}

void smartmeterwindow::on_pushButtonBack_clicked() {
    hide();
}
