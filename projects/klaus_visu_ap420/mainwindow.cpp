#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "statusled.h"

#include <QProcess>
#include <iostream>

#include <QMouseEvent>
#include <QColorDialog>
#include <QMqttClient>
#include <QString>
#include <QtCore/QDateTime>

#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow) {
    ui->setupUi(this);

    backlightIsAvailable =  false;
    backlightBrightness = new QFile("/sys/class/backlight/backlight/brightness");
    if (backlightBrightness && backlightBrightness->exists()) {
        backlightIsAvailable = backlightBrightness->open(QIODevice::ReadWrite);
        if (backlightIsAvailable) {
            backlightBrightness->write("6");
            backlightBrightness->flush();
        }
    }

    fbBlankIsAvailable =  false;
    fbBlank = new QFile("/sys/class/graphics/fb0/blank");
    if (fbBlank && fbBlank->exists()) {
        fbBlankIsAvailable = fbBlank->open(QIODevice::ReadWrite);
        if (fbBlankIsAvailable) {
            fbBlank->write("0");
            fbBlank->flush();
        }
    }

    // screen saver
    screensaverOn = false;
    scrTimer = new QTimer(this);
    connect(scrTimer, SIGNAL(timeout()), this, SLOT(scrTimerEvent()));
    scrTimer->setSingleShot(true);
    scrTimer->start(60000);

    // cyclic timer for measurement display
    roomTemperature = new QFile("/sys/class/hwmon/hwmon2/temp1_input");
    if (roomTemperature && roomTemperature->exists()) {
        roomTemperatureIsAvailable = true;
        roomTemperatureStr = new QString();
    } else {
        roomTemperatureIsAvailable = false;
    }

    roomHumidity = new QFile("/sys/class/hwmon/hwmon3/humidity1_input");
    if (roomHumidity && roomHumidity->exists()) {
        roomHumidityIsAvailable = true;
        roomHumidityStr = new QString();
    } else {
        roomHumidityIsAvailable = false;
    }

    statusLed = new statusled(this);
    statusLed->set_state(statusled::eOff);

    cycTimer = new QTimer(this);
    connect(cycTimer, SIGNAL(timeout()), this, SLOT(cycTimerEvent()));
    cycTimer->start(5000);

    qApp->installEventFilter(this);

    io = new ioState;

    uiEg = new egwindow(this, io);
    uiOg = new ogwindow(this, io);
    uiUg = new ugwindow(this, io);
    uiKueche = new kuechewindow(this, io);
    uiGarage = new garagewindow(this, io);
    uiSetup = new setupwindow(this, io);
    uiSmartmeter = new smartmeterwindow(this);
    uiKameraeingang = new kameraeingangwindow(this, io);

    mqttClient = new QMqttClient(this);
    mqttClient->setHostname(QString("10.0.0.200"));
    mqttClient->setPort(1883);
    mqttClient->connectToHost();

    connect(mqttClient, SIGNAL(connected()), this, SLOT(onMqtt_connected()));
    connect(mqttClient, SIGNAL(disconnected()), this, SLOT(onMqtt_disconnected()));
    connect(mqttClient, SIGNAL(messageReceived(const QByteArray &, const QMqttTopicName &)), this, SLOT(onMqtt_messageReceived(const QByteArray &, const QMqttTopicName &)));

    ui->pushButtonEG->setStyleSheet("background-color: green");
    ui->pushButtonOG->setStyleSheet("background-color: green");
    ui->pushButtonUG->setStyleSheet("background-color: green");
    ui->pushButtonGarage->setStyleSheet("background-color: green");
    ui->pushButtonKueche->setStyleSheet("background-color: green");
}

MainWindow::~MainWindow() {

    delete ui;
    delete uiEg;
    delete uiOg;
    delete uiUg;
    delete uiKueche;
    delete uiGarage;
    delete uiSetup;
    delete uiSmartmeter;
    delete io;

    if (backlightIsAvailable) {
        backlightBrightness->close();
        delete backlightBrightness;
    }
    if (fbBlankIsAvailable) {
        fbBlank->close();
        delete fbBlank;
    }
    if (roomTemperatureIsAvailable) {
        roomTemperature->close();
        delete roomTemperature;
    }
    if (roomHumidityIsAvailable) {
        roomHumidity->close();
        delete roomHumidity;
    }
    if (statusLed) {
        delete statusLed;
    }
}

void MainWindow::onMqtt_connected() {

    qDebug() << "mqtt connected";

    mqttClient->subscribe(QMqttTopicFilter("home/eg/arbeit/schreibtisch/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/arbeit/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/schrank/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/schlaf/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/bad/spiegel/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/bad/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/vorraum/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/wohn/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/wohn/lesen/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/vorraum/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/wc/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/glocke/taster/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/glocke/disable/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/ug/lager/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/ug/stiege/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/ug/arbeit/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/ug/fitness/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/ug/vorraum/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/ug/technik/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/gang/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/anna/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/severin/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/wc/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/kueche/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/kueche/wand/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/kueche/geschirrspueler/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/kueche/abwasch/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/kueche/kaffeemaschine/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/kueche/dunstabzug/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/speis/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eg/ess/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/stiege/netzteil/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/stiege/licht1/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/stiege/licht2/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/stiege/licht3/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/stiege/licht4/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/stiege/licht5/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/og/stiege/licht6/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/garage/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/garage/tor/status/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/terrasse/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eingang/licht/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/eingang/licht/mode/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/internet/actual"), 1);
    mqttClient->subscribe(QMqttTopicFilter("home/kamera/actual"), 1);
}
void MainWindow::onMqtt_disconnected() {

    qDebug() << "disconnected";
}

void MainWindow::scrTimerEvent() {
    screensaverOn = true;
    if (backlightIsAvailable) {
        backlightBrightness->write("0");
        backlightBrightness->flush();
    }
    if (fbBlankIsAvailable) {
        fbBlank->write("4");
        fbBlank->flush();
    }
    emit screenSaverActivated();
}

void MainWindow::cycTimerEvent() {

    if (roomTemperatureIsAvailable) {
        roomTemperature->open(QIODevice::ReadOnly);
        roomTemperatureStr->clear();
        roomTemperatureStr->append(roomTemperature->readLine());
        roomTemperatureStr->insert(roomTemperatureStr->size() - 4, '.');
        roomTemperatureStr->chop(3);
        roomTemperatureStr->append(" °C");
        roomTemperature->close();
        ui->temperature->setText(*roomTemperatureStr);
    }
    if (roomHumidityIsAvailable) {
        roomHumidity->open(QIODevice::ReadOnly);
        roomHumidityStr->clear();
        roomHumidityStr->append(roomHumidity->readLine());
        roomHumidityStr->chop(4);
        roomHumidityStr->append(" %rF");
        roomHumidity->close();
        ui->humidity->setText(*roomHumidityStr);
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {

   if (event->type() == QEvent::MouseButtonPress)
   {
       scrTimer->start();
       if (screensaverOn) {
           screensaverOn = false;
           if (fbBlankIsAvailable) {
               fbBlank->write("0");
               fbBlank->flush();
           }
           if (backlightIsAvailable) {
               backlightBrightness->write("6");
               backlightBrightness->flush();
           }
           return true;
       }
   }
  return false;
}

void MainWindow::onDisableScreenSaver(void) {

    QEvent event(QEvent::MouseButtonPress);
    eventFilter(nullptr, &event);
}

void MainWindow::onMessagePublish(const char *topic, const char *message) {

    mqttClient->publish(QMqttTopicName(topic), message, 1, false);
}


void MainWindow::messageActionStateBit(const QMqttTopicName &topic, const QByteArray &message, const char *compare_str, quint32 *state, quint32 mask) {

    if (QString::compare(topic.name(), compare_str) == 0) {
        if (QString::compare(message, "1") == 0) {
            *state |= mask;
        } else {
            *state &= ~mask;
        }
    }
}

void MainWindow::message_to_byteArray(const QString &message, QByteArray &data) {

    /* we get the message as readable hex array string, eg.: "01 02 55 aa" */
    int i;
    QStringList list = message.split(" ", QString::SkipEmptyParts);

    for (i = 0; i < list.size(); i++) {
        data.append(list.at(i).toUInt(0, 16));
    }
}

void MainWindow::messageActionVar(const QMqttTopicName &topic, const QByteArray &message, const char *compare_str, quint8 *data, quint8 data_len) {

    QByteArray buf;

    if (QString::compare(topic.name(), compare_str) == 0) {
        message_to_byteArray(QString(message), buf);
        if (buf.size() == data_len) {
            memcpy(data, buf.constData(), data_len);
        }
    }
}

void MainWindow::onMqtt_messageReceived(const QByteArray &message, const QMqttTopicName &topic) {

    quint32 egLight = io->egLight;
    quint32 ogLight = io->ogLight;
    quint32 ugLight = io->ugLight;
    quint32 kueche = io->kueche;
    quint32 garage = io->garage;
    quint32 sockets = io->sockets;
    quint32 glocke_taster = io->glocke_taster;
    quint32 var_glocke_disable = io->var_glocke_disable;
    quint32 var_mode_LightEingang = io->var_mode_LightEingang;

    qDebug() << topic.name() << ":" << message;

    messageActionStateBit(topic, message, "home/eg/arbeit/schreibtisch/actual", &io->egLight, ioState::egLightBits::egLightSchreibtisch);
    messageActionStateBit(topic, message, "home/eg/arbeit/licht/actual", &io->egLight, ioState::egLightBits::egLightArbeit);
    messageActionStateBit(topic, message, "home/eg/wohn/licht/actual", &io->egLight, ioState::egLightBits::egLightWohn);
    messageActionStateBit(topic, message, "home/eg/wohn/lesen/licht/actual", &io->egLight, ioState::egLightBits::egLightWohnLese);
    messageActionStateBit(topic, message, "home/eg/vorraum/licht/actual", &io->egLight, ioState::egLightBits::egLightVorraum);
    messageActionStateBit(topic, message, "home/eg/wc/licht/actual", &io->egLight, ioState::egLightBits::egLightWC);
    messageActionStateBit(topic, message, "home/eg/ess/licht/actual", &io->egLight, ioState::egLightBits::egLightEss);
    messageActionStateBit(topic, message, "home/eg/gang/licht/actual", &io->egLight, ioState::egLightBits::egLightGang);
    messageActionStateBit(topic, message, "home/terrasse/licht/actual", &io->egLight, ioState::egLightBits::egLightTerrasse);
    messageActionStateBit(topic, message, "home/eingang/licht/actual", &io->egLight, ioState::egLightBits::egLightEingang);

    messageActionStateBit(topic, message, "home/og/schrank/licht/actual", &io->ogLight, ioState::ogLightBits::ogLightSchrank);
    messageActionStateBit(topic, message, "home/og/schlaf/licht/actual", &io->ogLight, ioState::ogLightBits::ogLightSchlaf);
    messageActionStateBit(topic, message, "home/og/bad/licht/actual", &io->ogLight, ioState::ogLightBits::ogLightBad);
    messageActionStateBit(topic, message, "home/og/bad/spiegel/licht/actual", &io->ogLight, ioState::ogLightBits::ogLightBadSpiegel);
    messageActionStateBit(topic, message, "home/og/vorraum/licht/actual", &io->ogLight, ioState::ogLightBits::ogLightVorraum);
    messageActionStateBit(topic, message, "home/og/anna/licht/actual", &io->ogLight, ioState::ogLightBits::ogLightAnna);
    messageActionStateBit(topic, message, "home/og/severin/licht/actual", &io->ogLight, ioState::ogLightBits::ogLightSeverin);
    messageActionStateBit(topic, message, "home/og/wc/licht/actual", &io->ogLight, ioState::ogLightBits::ogLightWC);
    messageActionStateBit(topic, message, "home/og/stiege/netzteil/actual", &io->ogLight, ioState::ogLightBits::ogLightStiegePwr);
    messageActionStateBit(topic, message, "home/og/stiege/licht1/actual", &io->ogLight, ioState::ogLightBits::ogLightStiege1);
    messageActionStateBit(topic, message, "home/og/stiege/licht2/actual", &io->ogLight, ioState::ogLightBits::ogLightStiege2);
    messageActionStateBit(topic, message, "home/og/stiege/licht3/actual", &io->ogLight, ioState::ogLightBits::ogLightStiege3);
    messageActionStateBit(topic, message, "home/og/stiege/licht4/actual", &io->ogLight, ioState::ogLightBits::ogLightStiege4);
    messageActionStateBit(topic, message, "home/og/stiege/licht5/actual", &io->ogLight, ioState::ogLightBits::ogLightStiege5);
    messageActionStateBit(topic, message, "home/og/stiege/licht6/actual", &io->ogLight, ioState::ogLightBits::ogLightStiege6);

    messageActionStateBit(topic, message, "home/ug/lager/licht/actual", &io->ugLight, ioState::ugLightBits::ugLightLager);
    messageActionStateBit(topic, message, "home/ug/stiege/licht/actual", &io->ugLight, ioState::ugLightBits::ugLightStiege);
    messageActionStateBit(topic, message, "home/ug/fitness/licht/actual", &io->ugLight, ioState::ugLightBits::ugLightFitness);
    messageActionStateBit(topic, message, "home/ug/arbeit/licht/actual", &io->ugLight, ioState::ugLightBits::ugLightArbeit);
    messageActionStateBit(topic, message, "home/ug/vorraum/licht/actual", &io->ugLight, ioState::ugLightBits::ugLightVorraum);
    messageActionStateBit(topic, message, "home/ug/technik/licht/actual", &io->ugLight, ioState::ugLightBits::ugLightTechnik);

    messageActionStateBit(topic, message, "home/eg/kueche/licht/actual", &io->kueche, ioState::kuecheLightBits::kuecheLight);
    messageActionStateBit(topic, message, "home/eg/kueche/wand/licht/actual", &io->kueche, ioState::kuecheLightBits::kuecheLightWand);
    messageActionStateBit(topic, message, "home/eg/kueche/geschirrspueler/licht/actual", &io->kueche, ioState::kuecheLightBits::kuecheLightGeschirrspueler);
    messageActionStateBit(topic, message, "home/eg/kueche/abwasch/licht/actual", &io->kueche, ioState::kuecheLightBits::kuecheLightAbwasch);
    messageActionStateBit(topic, message, "home/eg/kueche/kaffeemaschine/licht/actual", &io->kueche, ioState::kuecheLightBits::kuecheLightKaffee);
    messageActionStateBit(topic, message, "home/eg/kueche/dunstabzug/licht/actual", &io->kueche, ioState::kuecheLightBits::kuecheLightDunstabzug);
    messageActionStateBit(topic, message, "home/eg/speis/licht/actual", &io->kueche, ioState::kuecheLightBits::kuecheLightSpeis);

    messageActionStateBit(topic, message, "home/garage/licht/actual", &io->garage, ioState::garageBits::garageLight);
    messageActionStateBit(topic, message, "home/garage/tor/status/actual", &io->garage, ioState::garageBits::garageDoorClosed);

    messageActionStateBit(topic, message, "home/internet/actual", &io->sockets, ioState::socketBits::socketInternet);
    messageActionStateBit(topic, message, "home/kamera/actual", &io->sockets, ioState::socketBits::socketCamera);

    messageActionStateBit(topic, message, "home/glocke/taster/actual", &io->glocke_taster, 1 /* 1 bit */);

    messageActionVar(topic, message, "home/glocke/disable/actual", &io->var_glocke_disable, 1);
    messageActionVar(topic, message, "home/eingang/licht/mode/actual", &io->var_mode_LightEingang, 1);

    if ((egLight != io->egLight)                             ||
        (ogLight != io->ogLight)                             ||
        (ugLight != io->ugLight)                             ||
        (kueche != io->kueche)                               ||
        (garage != io->garage)                               ||
        (sockets != io->sockets)                             ||
        (glocke_taster != io->glocke_taster)                 ||
        (var_glocke_disable != io->var_glocke_disable)       ||
        (var_mode_LightEingang != io->var_mode_LightEingang)) {
        emit ioChanged();
    }

    if (egLight != io->egLight) {
        if (!io->egLight) {
            ui->pushButtonEG->setStyleSheet("background-color: green");
        } else {
            ui->pushButtonEG->setStyleSheet("background-color: yellow");
        }
        ui->pushButtonEG->update();
    }

    if (ugLight != io->ugLight) {
        if (!io->ugLight) {
            ui->pushButtonUG->setStyleSheet("background-color: green");
        } else {
            ui->pushButtonUG->setStyleSheet("background-color: yellow");
        }
        ui->pushButtonUG->update();
    }

    if (ogLight != io->ogLight) {
        if (!io->ogLight) {
            ui->pushButtonOG->setStyleSheet("background-color: green");
        } else {
            ui->pushButtonOG->setStyleSheet("background-color: yellow");
        }
        ui->pushButtonOG->update();
    }

    if (garage != io->garage) {
        if (!(io->garage & ioState::garageBits::garageLight) &&
             (io->garage & ioState::garageBits::garageDoorClosed)) {
            ui->pushButtonGarage->setStyleSheet("background-color: green");
        } else {
            if ((io->garage & ioState::garageBits::garageDoorClosed)) {
                ui->pushButtonGarage->setStyleSheet("background-color: yellow");
            } else {
                ui->pushButtonGarage->setStyleSheet("background-color: red");
            }
        }
        ui->pushButtonGarage->update();
    }

    if (kueche != io->kueche) {
        if (!io->kueche) {
            ui->pushButtonKueche->setStyleSheet("background-color: green");
        } else {
            ui->pushButtonKueche->setStyleSheet("background-color: yellow");
        }
        ui->pushButtonKueche->update();
    }

    if (!io->egLight && !io->ogLight && !io->ugLight && !io->kueche &&
        (!(io->garage & ioState::garageBits::garageLight) && (io->garage & ioState::garageBits::garageDoorClosed))) {
        if (statusLed) statusLed->set_state(statusled::eGreen);
    } else if (!(io->garage & ioState::garageBits::garageDoorClosed)) {
        if (statusLed) statusLed->set_state(statusled::eRedBlink);
    } else {
        if (statusLed) statusLed->set_state(statusled::eOrange);
    }
}

void MainWindow::on_pushButtonEG_clicked() {
    uiEg->show();
}

void MainWindow::on_pushButtonOG_clicked() {
    uiOg->show();
}

void MainWindow::on_pushButtonUG_clicked() {
    uiUg->show();
}

void MainWindow::on_pushButtonKueche_clicked() {
    uiKueche->show();
}

void MainWindow::on_pushButtonGarage_clicked() {
    uiGarage->show();
}

void MainWindow::on_pushButtonSetup_clicked() {
    uiSetup->show();
}

void MainWindow::on_pushButtonSmartMeter_clicked() {
    uiSmartmeter->show();
}

void MainWindow::on_pushButtonKameraEingang_clicked() {
    uiKameraeingang->show();
}
