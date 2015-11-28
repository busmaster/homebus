#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "moduleservice.h"
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
        roomTemperatureIsAvailable = roomTemperature->open(QIODevice::ReadOnly);
        if (roomTemperatureIsAvailable) {
            roomTemperatureStr = new QString();
        }
    } else {
        roomTemperatureIsAvailable = false;
    }

    roomHumidity = new QFile("/sys/class/hwmon/hwmon3/humidity1_input");
    if (roomHumidity && roomHumidity->exists()) {
        roomHumidityIsAvailable = roomHumidity->open(QIODevice::ReadOnly);
        if (roomHumidityIsAvailable) {
            roomHumidityStr = new QString();
        }
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

    InitEventMonitor();

    mservice = new moduleservice;

    uiEg = new egwindow(this, io);
    uiOg = new ogwindow(this, io);
    uiUg = new ugwindow(this, io);
    uiGarage = new garagewindow(this, io);
}

MainWindow::~MainWindow() {

    eventMonitor->kill();
    eventMonitor->waitForFinished();
    delete eventMonitor;

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
        roomTemperature->seek(0);
        roomTemperatureStr->clear();
        roomTemperatureStr->append(roomTemperature->readLine());
        roomTemperatureStr->insert(roomTemperatureStr->size() - 4, '.');
        roomTemperatureStr->chop(3);
        roomTemperatureStr->append(" °C");
        ui->temperature->setText(*roomTemperatureStr);
    }
    if (roomHumidityIsAvailable) {
        roomHumidity->seek(0);
        roomHumidityStr->clear();
        roomHumidityStr->append(roomHumidity->readLine());
        roomHumidityStr->chop(4);
        roomHumidityStr->append(" %rF");
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


void MainWindow::InitEventMonitor(void) {

    eventMonitor = new QProcess;

    connect(eventMonitor, SIGNAL(readyRead()), this, SLOT(readStdOut()));
    connect(eventMonitor, SIGNAL(started()), this, SLOT(onStarted()));
    connect(eventMonitor, SIGNAL(finished(int)), this, SLOT(onFinished(int)));

#ifdef AP420
    QString program = "/root/git/homebus/tools/eventmonitor/bin/eventmonitor";
#else
    QString program = "/home/germana/Oeffentlich/git/homebus/tools/eventmonitor/bin/eventmonitor";
#endif
    QStringList arguments;
    arguments << "-c" << "/dev/hausbus0" << "-a" << "250";

    eventState = eEsWaitForStart;
    eventMonitor->start(program, arguments);
}

void MainWindow::onSendServiceCmd(const char *pCmd) {

    mservice->command(pCmd);
}

void MainWindow::readStdOut() {
  //  std::cout << QString(eventMonitor->readAllStandardOutput()).toUtf8().constData() << std::endl;

    int i;
    QByteArray ev = eventMonitor->readAllStandardOutput();
    QList<QByteArray> ev_lines = ev.split('\n');
    quint32 egSum = io->egState.sum;
    quint32 ogSum = io->ogState.sum;
    quint32 ugSum = io->ugState.sum;
    quint32 garageSum = io->garageState.sum;
    bool    socket_1 = io->socket_1;
    bool    socket_2 = io->socket_1;

    for (i = 0; i < ev_lines.size(); i++) {
        if (qstrcmp(ev_lines[i], "event address 240 device type DO31") == 0) {
            eventState = eEsWaitForDO31_240_Do;
            continue;
        } else if (qstrcmp(ev_lines[i], "event address 241 device type DO31") == 0) {
            eventState = eEsWaitForDO31_241_Do;
            continue;
        } else if (qstrcmp(ev_lines[i], "event address 36 device type SW8") == 0) {
            eventState = eEsWaitForSW8_1;
            continue;
        }
        switch (eventState) {
        case eEsWaitForStart:
            break;
        case eEsWaitForDO31_240_Do:
//            std::cout << "do 240: " << ev_lines[i].constData() << std::endl;
            (ev_lines[i].at(0) == '0')  ? io->ogState.detail.lightStiegePwr = 0  : io->ogState.detail.lightStiegePwr = 1;
            (ev_lines[i].at(1) == '0')  ? io->ogState.detail.lightStiege1 = 0    : io->ogState.detail.lightStiege1 = 1;
            (ev_lines[i].at(2) == '0')  ? io->ogState.detail.lightStiege2 = 0    : io->ogState.detail.lightStiege2 = 1;
            (ev_lines[i].at(3) == '0')  ? io->ogState.detail.lightStiege3 = 0    : io->ogState.detail.lightStiege3 = 1;
            (ev_lines[i].at(4) == '0')  ? io->ogState.detail.lightStiege4 = 0    : io->ogState.detail.lightStiege4 = 1;
            (ev_lines[i].at(5) == '0')  ? io->ogState.detail.lightStiege5 = 0    : io->ogState.detail.lightStiege5 = 1;
            (ev_lines[i].at(6) == '0')  ? io->ogState.detail.lightStiege6 = 0    : io->ogState.detail.lightStiege6 = 1;
            (ev_lines[i].at(7) == '0')  ? io->ogState.detail.lightSchrank = 0    : io->ogState.detail.lightSchrank = 1;
            (ev_lines[i].at(8) == '0')  ? io->ogState.detail.lightSchlaf = 0     : io->ogState.detail.lightSchlaf = 1;
            (ev_lines[i].at(9) == '0')  ? io->garageState.detail.light = 0       : io->garageState.detail.light = 1;
            (ev_lines[i].at(10) == '0') ? io->ogState.detail.lightBadSpiegel = 0 : io->ogState.detail.lightBadSpiegel = 1;
            (ev_lines[i].at(11) == '0') ? io->ogState.detail.lightVorraum = 0    : io->ogState.detail.lightVorraum = 1;
            (ev_lines[i].at(12) == '0') ? io->ogState.detail.lightBad = 0        : io->ogState.detail.lightBad = 1;
            (ev_lines[i].at(13) == '0') ? io->egState.detail.lightWohn = 0       : io->egState.detail.lightWohn = 1;
            (ev_lines[i].at(14) == '0') ? io->egState.detail.lightWohnLese = 0   : io->egState.detail.lightWohnLese = 1;
            (ev_lines[i].at(15) == '0') ? io->egState.detail.lightEss = 0        : io->egState.detail.lightEss = 1;
            (ev_lines[i].at(16) == '0') ? io->egState.detail.lightVorraum = 0    : io->egState.detail.lightVorraum = 1;
            (ev_lines[i].at(17) == '0') ? io->egState.detail.lightWC = 0         : io->egState.detail.lightWC = 1;
            (ev_lines[i].at(19) == '0') ? io->egState.detail.lightKueche = 0     : io->egState.detail.lightKueche = 1;
            (ev_lines[i].at(20) == '0') ? io->egState.detail.lightTerrasse = 0   : io->egState.detail.lightTerrasse = 1;
            (ev_lines[i].at(21) == '0') ? io->ugState.detail.lightLager = 0      : io->ugState.detail.lightLager  = 1;
            (ev_lines[i].at(22) == '0') ? io->ugState.detail.lightStiege = 0     : io->ugState.detail.lightStiege  = 1;
            (ev_lines[i].at(23) == '0') ? io->ugState.detail.lightArbeit = 0     : io->ugState.detail.lightArbeit  = 1;
            (ev_lines[i].at(24) == '0') ? io->ugState.detail.lightFitness = 0    : io->ugState.detail.lightFitness = 1;
            (ev_lines[i].at(25) == '0') ? io->ugState.detail.lightVorraum = 0    : io->ugState.detail.lightVorraum  = 1;
            (ev_lines[i].at(26) == '0') ? io->ugState.detail.lightTechnik = 0    : io->ugState.detail.lightTechnik  = 1;
            (ev_lines[i].at(27) == '0') ? io->socket_1 = false                   : io->socket_1 = true;
            (ev_lines[i].at(28) == '0') ? io->socket_2 = false                   : io->socket_2 = true;
            eventState = eEsWaitForDO31_240_Sh;
            break;
        case eEsWaitForDO31_241_Do:
//            std::cout << "do 241: " << ev_lines[i].constData() << std::endl;
            (ev_lines[i].at(24) == '0') ? io->egState.detail.lightSpeis = 0      : io->egState.detail.lightSpeis = 1;
            (ev_lines[i].at(25) == '0') ? io->egState.detail.lightGang = 0       : io->egState.detail.lightGang = 1;
            (ev_lines[i].at(26) == '0') ? io->egState.detail.lightArbeit = 0     : io->egState.detail.lightArbeit = 1;
            (ev_lines[i].at(27) == '0') ? io->ogState.detail.lightAnna = 0       : io->ogState.detail.lightAnna = 1;
            (ev_lines[i].at(28) == '0') ? io->ogState.detail.lightSeverin = 0    : io->ogState.detail.lightSeverin = 1;
            (ev_lines[i].at(29) == '0') ? io->ogState.detail.lightWC = 0         : io->ogState.detail.lightWC = 1;
            (ev_lines[i].at(30) == '0') ? io->egState.detail.lightKuecheWand = 0 : io->egState.detail.lightKuecheWand = 1;
            eventState = eEsWaitForDO31_241_Sh;
            break;
        case eEsWaitForDO31_240_Sh:
//            std::cout << "sh 240: " << ev_lines[i].constData() << std::endl;
            eventState = eEsWaitForStart;
            break;
        case eEsWaitForDO31_241_Sh:
//            std::cout << "sh 241: " << ev_lines[i].constData() << std::endl;
            eventState = eEsWaitForStart;
            break;
        case eEsWaitForSW8_1:
//            std::cout << "sw8 1: " << ev_lines[i].constData() << std::endl;
            (ev_lines[i].at(0) == '0') ? io->garageState.detail.door = 1         : io->garageState.detail.door = 0;
            eventState = eEsWaitForStart;
            break;
        }
    }

    if ((egSum != io->egState.sum) ||
        (ogSum != io->ogState.sum) ||
        (ugSum != io->ugState.sum) ||
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

    if (socket_1 != io->socket_1) {
        if (io->socket_1 == 0) {
            ui->pushButtonInternet->setStyleSheet("background-color: green");
        } else {
            ui->pushButtonInternet->setStyleSheet("background-color: yellow");
        }
    }
    if (socket_2 != io->socket_2) {


    }

    if ((io->egState.sum == 0) &&
        (io->ogState.sum == 0) &&
        (io->ugState.sum == 0) &&
        (io->garageState.sum == 0)) {
        if (statusLed) statusLed->set_state(statusled::eGreen);
    } else if (io->garageState.detail.door != 0) {
        if (statusLed) statusLed->set_state(statusled::eRedBlink);
    } else {
        if (statusLed) statusLed->set_state(statusled::eOrange);
    }
}

void MainWindow::onStarted(){
    std::cout << "Process started" << std::endl;
}

void MainWindow::onFinished(int sig) {
    std::cout << "Process finished: " << sig << std::endl;
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
        doState[27] = 2; // on
    } else {
        doState[27] = 3; // off
    }
    do31Cmd(240, doState, sizeof(doState), command, sizeof(command));
    ui->pushButtonInternet->setStyleSheet("background-color: grey");

    onSendServiceCmd(command);
}
