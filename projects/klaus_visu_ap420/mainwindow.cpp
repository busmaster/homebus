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
#include <QJsonDocument>
#include <QJsonObject>

#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow) {

    QString name;
    ui->setupUi(this);

    backlightBrightness = new QFile("/sys/class/backlight/backlight/brightness");
    if (backlightBrightness) {
        if (backlightBrightness->exists()) {
            backlightBrightness->open(QIODevice::ReadWrite);
            backlightBrightness->write("6");
            backlightBrightness->flush();
        } else {
            delete backlightBrightness;
            backlightBrightness = 0;
        }
    }

    fbBlank = new QFile("/sys/class/graphics/fb0/blank");
    if (fbBlank) {
        if (fbBlank->exists()) {
            fbBlank->open(QIODevice::ReadWrite);
            fbBlank->write("0");
            fbBlank->flush();
        } else {
            delete fbBlank;
            fbBlank = 0;
        }
    }

    // screen saver
    screensaverOn = false;
    scrTimer = new QTimer(this);
    connect(scrTimer, SIGNAL(timeout()), this, SLOT(scrTimerEvent()));
    scrTimer->setSingleShot(true);
    scrTimer->start(60000);

    // cyclic timer for measurement display
    roomTemperature = 0;
    roomTemperatureStr = 0;
    if (get_hwmon_dev("roomtemp", name)) {
        roomTemperature = new QFile(name + "temp1_input");
        if (roomTemperature) {
            if (roomTemperature->exists()) {
                roomTemperatureStr = new QString();
            } else {
                delete roomTemperature;
                roomTemperature = 0;
            }
        }
    }

    roomHumidity = 0;
    roomHumidityStr = 0;
    if (get_hwmon_dev("humidity", name)) {
        roomHumidity = new QFile(name + "humidity1_input");
        if (roomHumidity) {
            if (roomHumidity->exists()) {
                roomHumidityStr = new QString();
            } else {
                delete roomHumidity;
                roomHumidity = 0;
            }
        }
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
    uiSmartmeter = new smartmeterwindow(this, io);
    uiKameraeingang = new kameraeingangwindow(this, io);
    uiDoor = new doorwindow(this, io);

    mqttReconnectTimer = 0;
    mqttClient = new QMqttClient(this);
    mqttClient->setHostname(QString("10.0.0.200"));
    mqttClient->setPort(1883);
    connect(mqttClient, SIGNAL(connected()), this, SLOT(onMqtt_connected()));
    connect(mqttClient, SIGNAL(disconnected()), this, SLOT(onMqtt_disconnected()));
    connect(mqttClient, SIGNAL(messageReceived(QByteArray,QMqttTopicName)), this, SLOT(onMqtt_messageReceived(QByteArray,QMqttTopicName)));
    mqttClient->connectToHost();

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
    delete uiDoor;
    delete io;

    if (backlightBrightness) {
        backlightBrightness->close();
        delete backlightBrightness;
        backlightBrightness = 0;
    }
    if (fbBlank) {
        fbBlank->close();
        delete fbBlank;
        fbBlank = 0;
    }
    if (roomTemperature) {
        roomTemperature->close();
        delete roomTemperature;
        roomTemperature = 0;
    }
    if (roomTemperatureStr) {
        delete roomTemperatureStr;
        roomTemperatureStr = 0;
    }
    if (roomHumidity) {
        roomHumidity->close();
        delete roomHumidity;
        roomHumidity = 0;
    }
    if (roomHumidityStr) {
        delete roomHumidityStr;
        roomHumidityStr = 0;
    }
    if (statusLed) {
        delete statusLed;
    }
}

#define MAX_HWMON 10
bool MainWindow::get_hwmon_dev(const QString &name, QString &result) {

    int i;
    bool rc = false;

    for (i = 0; i < MAX_HWMON; i++) {
        QString filename("/sys/class/hwmon/hwmon");
        filename.append(QString::number(i));
        filename.append("/name");
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream in(&file);
            QString line = in.readLine();
            file.close();
            if (!line.isNull() && QString::compare(line, name) == 0) {
                result = "/sys/class/hwmon/hwmon" + QString::number(i) + "/";
                rc = true;
                break;
            }
        } else {
            break;
        }
    }
    return rc;
}

void MainWindow::mqtt_subscribe(const QString &topic, int index) {

   mqttClient->subscribe(QMqttTopicFilter(topic), 1);
   topic_hash[topic] = index;

}

void MainWindow::onMqtt_connected() {

    qDebug() << "mqtt connected";
    if (mqttReconnectTimer) {
        mqttReconnectTimer->stop();
        delete mqttReconnectTimer;
        mqttReconnectTimer = 0;
        topic_hash.clear();
    }

    mqtt_subscribe("home/eg/arbeit/schreibtisch/actual", 0);
    mqtt_subscribe("home/eg/arbeit/licht/actual", 1);
    mqtt_subscribe("home/eg/wohn/licht/actual", 2);
    mqtt_subscribe("home/eg/wohn/lesen/licht/actual", 3);
    mqtt_subscribe("home/eg/vorraum/licht/actual", 4);
    mqtt_subscribe("home/eg/gang/licht/actual", 5);
    mqtt_subscribe("home/eg/wc/licht/actual", 6);
    mqtt_subscribe("home/eg/ess/licht/actual", 7);
    mqtt_subscribe("home/eg/speis/licht/actual", 8);
    mqtt_subscribe("home/eg/kueche/licht/actual", 9);
    mqtt_subscribe("home/eg/kueche/wand/licht/actual", 10);
    mqtt_subscribe("home/eg/kueche/geschirrspueler/licht/actual", 11);
    mqtt_subscribe("home/eg/kueche/abwasch/licht/actual", 12);
    mqtt_subscribe("home/eg/kueche/kaffeemaschine/licht/actual", 13);
    mqtt_subscribe("home/eg/kueche/dunstabzug/licht/actual", 14);

    mqtt_subscribe("home/og/schrank/licht/actual", 20);
    mqtt_subscribe("home/og/schlaf/licht/actual", 21);
    mqtt_subscribe("home/og/bad/spiegel/licht/actual", 22);
    mqtt_subscribe("home/og/bad/licht/actual", 23);
    mqtt_subscribe("home/og/vorraum/licht/actual", 24);
    mqtt_subscribe("home/og/anna/licht/actual", 25);
    mqtt_subscribe("home/og/severin/licht/actual", 26);
    mqtt_subscribe("home/og/wc/licht/actual", 27);
    mqtt_subscribe("home/og/stiege/netzteil/actual", 28);
    mqtt_subscribe("home/og/stiege/licht1/actual", 29);
    mqtt_subscribe("home/og/stiege/licht2/actual", 30);
    mqtt_subscribe("home/og/stiege/licht3/actual", 31);
    mqtt_subscribe("home/og/stiege/licht4/actual", 32);
    mqtt_subscribe("home/og/stiege/licht5/actual", 33);
    mqtt_subscribe("home/og/stiege/licht6/actual", 34);

    mqtt_subscribe("home/ug/lager/licht/actual", 40);
    mqtt_subscribe("home/ug/stiege/licht/actual", 41);
    mqtt_subscribe("home/ug/arbeit/licht/actual", 42);
    mqtt_subscribe("home/ug/fitness/licht/actual", 43);
    mqtt_subscribe("home/ug/vorraum/licht/actual", 44);
    mqtt_subscribe("home/ug/technik/licht/actual", 45);

    mqtt_subscribe("home/glocke/taster/actual", 50);
    mqtt_subscribe("home/glocke/disable/actual", 51);

    mqtt_subscribe("home/garage/licht/actual", 60);
    mqtt_subscribe("home/garage/tor/status/actual", 61);

    mqtt_subscribe("home/terrasse/licht/actual", 70);

    mqtt_subscribe("home/eingang/licht/actual", 80);
    mqtt_subscribe("home/eingang/licht/mode/actual", 81);

    mqtt_subscribe("home/internet/actual", 90);
    mqtt_subscribe("home/kamera/actual", 91);

    mqtt_subscribe("home/smartmeter/actual", 100);

    mqtt_subscribe("home/door/lock/actual", 110);

    mqtt_subscribe("home/smartmeter/midnight/consumed", 120);
    mqtt_subscribe("home/smartmeter/midnight/produced", 121);
    mqtt_subscribe("solar/114181014906/1/yieldday", 122);
    mqtt_subscribe("solar/114181014906/2/yieldday", 123);
    mqtt_subscribe("solar/114183081983/1/yieldday", 124);
    mqtt_subscribe("solar/114183081983/2/yieldday", 125);
    mqtt_subscribe("solar/114183084705/1/yieldday", 126);
    mqtt_subscribe("solar/114183084705/2/yieldday", 127);
    mqtt_subscribe("solar/114181014906/0/power", 128);
    mqtt_subscribe("solar/114183081983/0/power", 129);
    mqtt_subscribe("solar/114183084705/0/power", 130);
}
void MainWindow::onMqtt_disconnected() {

    qDebug() << "disconnected";

    mqttReconnectTimer = new QTimer(this);
    connect(mqttReconnectTimer, SIGNAL(timeout()), this, SLOT(mqttReconnectTimerEvent()));
    mqttReconnectTimer->start(5000);

}

void MainWindow::mqttReconnectTimerEvent() {

    qDebug() << "reconnect timer";
    mqttClient->connectToHost();
}

void MainWindow::scrTimerEvent() {
    screensaverOn = true;
    if (backlightBrightness) {
        backlightBrightness->write("0");
        backlightBrightness->flush();
    }
    if (fbBlank) {
        fbBlank->write("4");
        fbBlank->flush();
    }
    emit screenSaverActivated();
}

void MainWindow::cycTimerEvent() {

    if (roomTemperature) {
        roomTemperature->open(QIODevice::ReadOnly);
        roomTemperatureStr->clear();
        roomTemperatureStr->append(roomTemperature->readLine());
        roomTemperatureStr->insert(roomTemperatureStr->size() - 4, '.');
        roomTemperatureStr->chop(3);
        roomTemperatureStr->append(" °C");
        roomTemperature->close();
        ui->temperature->setText(*roomTemperatureStr);
    }
    if (roomHumidity) {
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

    (void)obj;

    if (event->type() == QEvent::MouseButtonPress) {
        scrTimer->start();
        if (screensaverOn) {
            screensaverOn = false;
            if (fbBlank) {
                fbBlank->write("0");
                fbBlank->flush();
            }
            if (backlightBrightness) {
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


void MainWindow::messageActionStateBit(const QByteArray &message, quint32 *state, quint32 mask) {

   if (QString::compare(message, "1") == 0) {
       *state |= mask;
   } else {
       *state &= ~mask;
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

void MainWindow::messageActionVar(const QByteArray &message, quint8 *data, quint8 data_len) {

    QByteArray buf;

    message_to_byteArray(QString(message), buf);
    if (buf.size() == data_len) {
        memcpy(data, buf.constData(), data_len);
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
    bool smChanged = false;
    bool doorStateChanged = false;
    bool solarChanged = false;
    int index = topic_hash[topic.name()];
    QList<QByteArray> l;

//    qDebug() << topic.name() << ": " << index << ": " << message ;

    switch (index) {
    case 0:  messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightSchreibtisch); break;
    case 1:  messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightArbeit); break;
    case 2:  messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightWohn); break;
    case 3:  messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightWohnLese); break;
    case 4:  messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightVorraum); break;
    case 5:  messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightGang); break;
    case 6:  messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightWC); break;
    case 7:  messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightEss); break;
    case 8:  messageActionStateBit(message, &io->kueche, ioState::kuecheLightBits::kuecheLightSpeis); break;
    case 9:  messageActionStateBit(message, &io->kueche,  ioState::kuecheLightBits::kuecheLight); break;
    case 10: messageActionStateBit(message, &io->kueche,  ioState::kuecheLightBits::kuecheLightWand); break;
    case 11: messageActionStateBit(message, &io->kueche,  ioState::kuecheLightBits::kuecheLightGeschirrspueler); break;
    case 12: messageActionStateBit(message, &io->kueche,  ioState::kuecheLightBits::kuecheLightAbwasch); break;
    case 13: messageActionStateBit(message, &io->kueche,  ioState::kuecheLightBits::kuecheLightKaffee); break;
    case 14: messageActionStateBit(message, &io->kueche,  ioState::kuecheLightBits::kuecheLightDunstabzug); break;

    case 20: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightSchrank); break;
    case 21: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightSchlaf); break;
    case 22: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightBadSpiegel); break;
    case 23: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightBad); break;
    case 24: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightVorraum); break;
    case 25: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightAnna); break;
    case 26: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightSeverin); break;
    case 27: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightWC); break;
    case 28: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightStiegePwr); break;
    case 29: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightStiege1); break;
    case 30: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightStiege2); break;
    case 31: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightStiege3); break;
    case 32: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightStiege4); break;
    case 33: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightStiege5); break;
    case 34: messageActionStateBit(message, &io->ogLight,  ioState::ogLightBits::ogLightStiege6); break;

    case 40: messageActionStateBit(message, &io->ugLight,  ioState::ugLightBits::ugLightLager); break;
    case 41: messageActionStateBit(message, &io->ugLight,  ioState::ugLightBits::ugLightStiege); break;
    case 42: messageActionStateBit(message, &io->ugLight,  ioState::ugLightBits::ugLightArbeit); break;
    case 43: messageActionStateBit(message, &io->ugLight,  ioState::ugLightBits::ugLightFitness); break;
    case 44: messageActionStateBit(message, &io->ugLight,  ioState::ugLightBits::ugLightVorraum); break;
    case 45: messageActionStateBit(message, &io->ugLight,  ioState::ugLightBits::ugLightTechnik); break;

    case 50: messageActionStateBit(message, &io->glocke_taster, 1); break;
    case 51: messageActionVar(message, &io->var_glocke_disable, 1); break;

    case 60: messageActionStateBit(message, &io->garage, ioState::garageBits::garageLight); break;
    case 61: messageActionStateBit(message, &io->garage, ioState::garageBits::garageDoorClosed); break;

    case 70: messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightTerrasse); break;

    case 80: messageActionStateBit(message, &io->egLight, ioState::egLightBits::egLightEingang); break;
    case 81: messageActionVar(message, &io->var_mode_LightEingang, 1); break;

    case 90: messageActionStateBit(message, &io->sockets, ioState::socketBits::socketInternet); break;
    case 91: messageActionStateBit(message, &io->sockets, ioState::socketBits::socketCamera); break;

    case 100: {
        QJsonDocument doc = QJsonDocument::fromJson(message);
        QJsonObject jObject = doc.object();
        QJsonValue response;
        QJsonObject responseObj;

        response = jObject.value("counter");
        responseObj = response.toObject();
        io->sm.a_plus = responseObj.value("A+").toInt();
        io->sm.a_minus = responseObj.value("A-").toInt();
        io->sm.r_plus = responseObj.value("R+").toInt();;
        io->sm.r_minus = responseObj.value("R-").toInt();
        response = jObject.value("power");
        responseObj = response.toObject();
        io->sm.p_plus = responseObj.value("P+").toInt();
        io->sm.p_minus = responseObj.value("P-").toInt();
        io->sm.q_plus = responseObj.value("Q+").toInt();
        io->sm.q_minus = responseObj.value("Q-").toInt();
        smChanged = true;
        break;
    }
    case 110:
        if (QString::compare(message, "locked") == 0) {
            io->door.lockstate = ioState::doorState::locked;
        } else if (QString::compare(message, "unlocked") == 0) {
            io->door.lockstate = ioState::doorState::unlocked;
        } else if (QString::compare(message, "internal") == 0) {
            io->door.lockstate = ioState::doorState::internal;
        } else if (QString::compare(message, "invalid1") == 0) {
            io->door.lockstate = ioState::doorState::invalid1;
        } else if (QString::compare(message, "invalid2") == 0) {
            io->door.lockstate = ioState::doorState::invalid2;
        } else if (QString::compare(message, "noresp") == 0) {
            io->door.lockstate = ioState::doorState::noresp;
        } else if (QString::compare(message, "noconnection") == 0) {
            io->door.lockstate = ioState::doorState::noconnection;
        } else if (QString::compare(message, "uncalib") == 0) {
            io->door.lockstate = ioState::doorState::uncalib;
        } else if (QString::compare(message, "again") == 0) {
            io->door.lockstate = ioState::doorState::again;
        }
        doorStateChanged = true;
        break;
    case 120:
        io->sm.a_plus_midnight = message.toInt();
        smChanged = true;
        break;
    case 121:
        io->sm.a_minus_midnight = message.toInt();
        smChanged = true;
        break;
    case 122:{
        l = message.split('.');
        if (l.count() == 1) {
            io->sy.oben_ost = l[0].toInt();
        } else if (l.count() == 2) {
            io->sy.oben_ost = l[0].toInt() * 1000 + l[1].toInt();
        } else {
            io->sy.oben_ost = 0;
        }
        solarChanged = true;
    }
        break;
    case 123:
        l = message.split('.');
        if (l.count() == 1) {
            io->sy.oben_mitte = l[0].toInt();
        } else if (l.count() == 2) {
            io->sy.oben_mitte = l[0].toInt() * 1000 + l[1].toInt();
        } else {
            io->sy.oben_mitte = 0;
        }
        solarChanged = true;
        break;
    case 124:
        l = message.split('.');
        if (l.count() == 1) {
            io->sy.oben_west = l[0].toInt();
        } else if (l.count() == 2) {
            io->sy.oben_west = l[0].toInt() * 1000 + l[1].toInt();
        } else {
            io->sy.oben_west = 0;
        }
        solarChanged = true;
        break;
    case 125:
        l = message.split('.');
        if (l.count() == 1) {
            io->sy.unten_ost = l[0].toInt();
        } else if (l.count() == 2) {
            io->sy.unten_ost = l[0].toInt() * 1000 + l[1].toInt();
        } else {
            io->sy.unten_ost = 0;
        }
        solarChanged = true;
        break;
    case 126:
        l = message.split('.');
        if (l.count() == 1) {
            io->sy.unten_mitte = l[0].toInt();
        } else if (l.count() == 2) {
            io->sy.unten_mitte = l[0].toInt() * 1000 + l[1].toInt();
        } else {
            io->sy.unten_mitte = 0;
        }
        solarChanged = true;
        break;
    case 127:
        l = message.split('.');
        if (l.count() == 1) {
            io->sy.unten_west = l[0].toInt();
        } else if (l.count() == 2) {
            io->sy.unten_west = l[0].toInt() * 1000 + l[1].toInt();
        } else {
            io->sy.unten_west = 0;
        }
        solarChanged = true;
        break;
    case 128:
        l = message.split('.');
        if (l.count() == 1) {
            io->sp.rechts = l[0].toInt();
        } else if (l.count() == 2) {
            io->sp.rechts = l[0].toInt() * 10 + l[1].toInt();
        } else {
            io->sp.rechts = 0;
        }
        solarChanged = true;
        break;
    case 129:
        l = message.split('.');
        if (l.count() == 1) {
            io->sp.mitte= l[0].toInt();
        } else if (l.count() == 2) {
            io->sp.mitte = l[0].toInt() * 10 + l[1].toInt();
        } else {
            io->sp.mitte = 0;
        }
        solarChanged = true;
        break;
    case 130:
        l = message.split('.');
        if (l.count() == 1) {
            io->sp.links = l[0].toInt();
        } else if (l.count() == 2) {
            io->sp.links = l[0].toInt() * 10 + l[1].toInt();
        } else {
            io->sp.links = 0;
        }
        solarChanged = true;
        break;
    default:
        break;
    }

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

    if (doorStateChanged) {
        emit doorChanged();
    }

    if (solarChanged || smChanged) {
        emit meterChanged();
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

void MainWindow::on_pushButtonDoor_clicked() {
    uiDoor->show();
}
