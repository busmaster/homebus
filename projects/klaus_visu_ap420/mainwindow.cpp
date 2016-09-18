#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "moduleservice.h"
#include "eventmonitor.h"
#include "statusled.h"

#include <QProcess>
#include <iostream>

#include <QMouseEvent>
#include <QColorDialog>

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

    meventmon = new eventmonitor;
    connect(meventmon, SIGNAL(busEvent(struct eventmonitor::event *)), this, SLOT(onBusEvent(struct eventmonitor::event *)));

    mservice = new moduleservice;

    uiEg = new egwindow(this, io);
    uiOg = new ogwindow(this, io);
    uiUg = new ugwindow(this, io);
    uiKueche = new kuechewindow(this, io);
    uiGarage = new garagewindow(this, io);
}

MainWindow::~MainWindow() {

    meventmon->command("-exit\n");
    meventmon->waitForFinished();
    delete meventmon;

    mservice->command("-exit\n");
    mservice->waitForFinished();
    delete mservice;

    delete ui;
    delete uiEg;
    delete uiOg;
    delete uiUg;
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
   obj = obj;

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

void MainWindow::onSendServiceCmd(const char *pCmd) {

    mservice->command(pCmd);
}

void MainWindow::onBusEvent(eventmonitor::event *ev) {

    quint32 egSum = io->egState.sum;
    quint32 ogSum = io->ogState.sum;
    quint32 ugSum = io->ugState.sum;
    quint32 kuecheSum = io->kuecheState.sum;
    quint32 garageSum = io->garageState.sum;
    bool    socket_1 = io->socket_1;
    bool    socket_2 = io->socket_1;

    if ((ev->srcAddr == 240) && (ev->type == eventmonitor::eDevDo31)) {
        ((ev->data.do31.digOut & 0x00000001) == 0) ? io->ogState.detail.lightStiegePwr = 0  : io->ogState.detail.lightStiegePwr = 1;
        ((ev->data.do31.digOut & 0x00000002) == 0) ? io->ogState.detail.lightStiege1 = 0    : io->ogState.detail.lightStiege1 = 1;
        ((ev->data.do31.digOut & 0x00000004) == 0) ? io->ogState.detail.lightStiege2 = 0    : io->ogState.detail.lightStiege2 = 1;
        ((ev->data.do31.digOut & 0x00000008) == 0) ? io->ogState.detail.lightStiege3 = 0    : io->ogState.detail.lightStiege3 = 1;
        ((ev->data.do31.digOut & 0x00000010) == 0) ? io->ogState.detail.lightStiege4 = 0    : io->ogState.detail.lightStiege4 = 1;
        ((ev->data.do31.digOut & 0x00000020) == 0) ? io->ogState.detail.lightStiege5 = 0    : io->ogState.detail.lightStiege5 = 1;
        ((ev->data.do31.digOut & 0x00000040) == 0) ? io->ogState.detail.lightStiege6 = 0    : io->ogState.detail.lightStiege6 = 1;
        ((ev->data.do31.digOut & 0x00000080) == 0) ? io->ogState.detail.lightSchrank = 0    : io->ogState.detail.lightSchrank = 1;
        ((ev->data.do31.digOut & 0x00000100) == 0) ? io->ogState.detail.lightSchlaf = 0     : io->ogState.detail.lightSchlaf = 1;
        ((ev->data.do31.digOut & 0x00000200) == 0) ? io->garageState.detail.light = 0       : io->garageState.detail.light = 1;
        ((ev->data.do31.digOut & 0x00000400) == 0) ? io->ogState.detail.lightBadSpiegel = 0 : io->ogState.detail.lightBadSpiegel = 1;
        ((ev->data.do31.digOut & 0x00000800) == 0) ? io->ogState.detail.lightVorraum = 0    : io->ogState.detail.lightVorraum = 1;
        ((ev->data.do31.digOut & 0x00001000) == 0) ? io->ogState.detail.lightBad = 0        : io->ogState.detail.lightBad = 1;
        ((ev->data.do31.digOut & 0x00002000) == 0) ? io->egState.detail.lightWohn = 0       : io->egState.detail.lightWohn = 1;
        ((ev->data.do31.digOut & 0x00004000) == 0) ? io->egState.detail.lightWohnLese = 0   : io->egState.detail.lightWohnLese = 1;
        ((ev->data.do31.digOut & 0x00008000) == 0) ? io->egState.detail.lightEss = 0        : io->egState.detail.lightEss = 1;
        ((ev->data.do31.digOut & 0x00010000) == 0) ? io->egState.detail.lightVorraum = 0    : io->egState.detail.lightVorraum = 1;
        ((ev->data.do31.digOut & 0x00020000) == 0) ? io->egState.detail.lightWC = 0         : io->egState.detail.lightWC = 1;
        ((ev->data.do31.digOut & 0x00080000) == 0) ? io->kuecheState.detail.light = 0       : io->kuecheState.detail.light = 1;
        ((ev->data.do31.digOut & 0x00100000) == 0) ? io->egState.detail.lightTerrasse = 0   : io->egState.detail.lightTerrasse = 1;
        ((ev->data.do31.digOut & 0x00200000) == 0) ? io->ugState.detail.lightLager = 0      : io->ugState.detail.lightLager  = 1;
        ((ev->data.do31.digOut & 0x00400000) == 0) ? io->ugState.detail.lightStiege = 0     : io->ugState.detail.lightStiege  = 1;
        ((ev->data.do31.digOut & 0x00800000) == 0) ? io->ugState.detail.lightArbeit = 0     : io->ugState.detail.lightArbeit  = 1;
        ((ev->data.do31.digOut & 0x01000000) == 0) ? io->ugState.detail.lightFitness = 0    : io->ugState.detail.lightFitness = 1;
        ((ev->data.do31.digOut & 0x02000000) == 0) ? io->ugState.detail.lightVorraum = 0    : io->ugState.detail.lightVorraum  = 1;
        ((ev->data.do31.digOut & 0x04000000) == 0) ? io->ugState.detail.lightTechnik = 0    : io->ugState.detail.lightTechnik  = 1;
        ((ev->data.do31.digOut & 0x08000000) == 0) ? io->socket_1 = true                    : io->socket_1 = false; // inverted connection NC
        ((ev->data.do31.digOut & 0x10000000) == 0) ? io->socket_2 = true                    : io->socket_2 = false; // inverted connection NC
        ((ev->data.do31.digOut & 0x20000000) == 0) ? io->kuecheState.detail.lightAbwasch = 0: io->kuecheState.detail.lightAbwasch = 1;
    } else if ((ev->srcAddr == 241) && (ev->type == eventmonitor::eDevDo31)) {
        ((ev->data.do31.digOut & 0x01000000) == 0) ? io->kuecheState.detail.lightSpeis = 0  : io->kuecheState.detail.lightSpeis = 1;
        ((ev->data.do31.digOut & 0x02000000) == 0) ? io->egState.detail.lightGang = 0       : io->egState.detail.lightGang = 1;
        ((ev->data.do31.digOut & 0x04000000) == 0) ? io->egState.detail.lightArbeit = 0     : io->egState.detail.lightArbeit = 1;
        ((ev->data.do31.digOut & 0x08000000) == 0) ? io->ogState.detail.lightAnna = 0       : io->ogState.detail.lightAnna = 1;
        ((ev->data.do31.digOut & 0x10000000) == 0) ? io->ogState.detail.lightSeverin = 0    : io->ogState.detail.lightSeverin = 1;
        ((ev->data.do31.digOut & 0x20000000) == 0) ? io->ogState.detail.lightWC = 0         : io->ogState.detail.lightWC = 1;
        ((ev->data.do31.digOut & 0x40000000) == 0) ? io->kuecheState.detail.lightWand = 0   : io->kuecheState.detail.lightWand = 1;
    } else if ((ev->srcAddr == 36) && (ev->type == eventmonitor::eDevSw8)) {
        ((ev->data.sw8.digInOut & 0x01) == 0)      ? io->garageState.detail.door = 1         : io->garageState.detail.door = 0;
    }

    if ((egSum != io->egState.sum) ||
        (ogSum != io->ogState.sum) ||
        (ugSum != io->ugState.sum) ||
        (kuecheSum != io->kuecheState.sum) ||
        (garageSum != io->garageState.sum)) {
        emit ioChanged();
    }

    if (egSum != io->egState.sum) {
        if (io->egState.sum == 0) {
            ui->pushButtonEG->setStyleSheet("background-color: green");
        } else {
            ui->pushButtonEG->setStyleSheet("background-color: yellow");
        }
        ui->pushButtonEG->update();
    }

    if (ugSum != io->ugState.sum) {
        if (io->ugState.sum == 0) {
            ui->pushButtonUG->setStyleSheet("background-color: green");
        } else {
            ui->pushButtonUG->setStyleSheet("background-color: yellow");
        }
        ui->pushButtonUG->update();
    }

    if (ogSum != io->ogState.sum) {
        if (io->ogState.sum == 0) {
            ui->pushButtonOG->setStyleSheet("background-color: green");
        } else {
            ui->pushButtonOG->setStyleSheet("background-color: yellow");
        }
        ui->pushButtonOG->update();
    }

    if (garageSum != io->garageState.sum) {
        if (io->garageState.sum == 0) {
            ui->pushButtonGarage->setStyleSheet("background-color: green");
        } else {
            if (io->garageState.detail.door == 0) {
                ui->pushButtonGarage->setStyleSheet("background-color: yellow");
            } else {
                ui->pushButtonGarage->setStyleSheet("background-color: red");
            }
        }
        ui->pushButtonGarage->update();
    }

    if (kuecheSum != io->kuecheState.sum) {
        if (io->kuecheState.sum == 0) {
            ui->pushButtonKueche->setStyleSheet("background-color: green");
        } else {
            ui->pushButtonKueche->setStyleSheet("background-color: yellow");
        }
        ui->pushButtonKueche->update();
    }

    if (socket_1 != io->socket_1) {
        if (io->socket_1) {
            ui->pushButtonInternet->setStyleSheet("background-color: yellow");
        } else {
            ui->pushButtonInternet->setStyleSheet("background-color: green");
        }
    }
    if (socket_2 != io->socket_2) {


    }

    if ((io->egState.sum == 0) &&
        (io->ogState.sum == 0) &&
        (io->ugState.sum == 0) &&
        (io->kuecheState.sum == 0) &&
        (io->garageState.sum == 0)) {
        if (statusLed) statusLed->set_state(statusled::eGreen);
    } else if (io->garageState.detail.door != 0) {
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

int MainWindow::do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen) {
    size_t i;
    int len;

    len = snprintf(pCmd, cmdLen, "-a %d -setvaldo31_do", do31Addr);

    for (i = 0; i < stateLen; i++) {
        len += snprintf(pCmd + len, cmdLen - len, " %d", *(pDoState + i));
    }
    return len;
}

void MainWindow::on_pushButtonInternet_pressed() {
    char    command[100];
    uint8_t doState[31];

    memset(doState, 0, sizeof(doState));
    if (io->socket_1) {
        doState[27] = 3; // on
    } else {
        doState[27] = 2; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonInternet->setStyleSheet("background-color: grey");

    onSendServiceCmd(command);
}
