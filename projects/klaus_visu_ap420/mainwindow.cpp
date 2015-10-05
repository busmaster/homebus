#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QProcess>
#include <iostream>

#include <QMouseEvent>
#include <QColorDialog>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
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
            QByteArray line = roomTemperature->readLine();
            ui->temperature->setText((QString)line);
        }
    }

    roomHumidity = new QFile("/sys/class/hwmon/hwmon3/humidity1_input");
    if (roomHumidity && roomHumidity->exists()) {
        roomHumidityIsAvailable = roomHumidity->open(QIODevice::ReadOnly);
        if (roomHumidityIsAvailable) {
            QByteArray line = roomHumidity->readLine();
            ui->humidity->setText((QString)line);
        }
    }

    cycTimer = new QTimer(this);
    connect(cycTimer, SIGNAL(timeout()), this, SLOT(cycTimerEvent()));
    cycTimer->start(5000);

    qApp->installEventFilter(this);

    io = new ioState;

    InitEventMonitor();

    uiEg = new egwindow(this, io);
    uiOg = new ogwindow(this, io);
    uiUg = new ugwindow(this, io);
    uiGarage = new garagewindow(this, io);
}

MainWindow::~MainWindow()
{
    eventMonitor->kill();
    eventMonitor->waitForFinished();
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
}

void MainWindow::scrTimerEvent()
{
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
        QByteArray line = roomTemperature->readLine();
        ui->temperature->setText((QString)line);
    }
    if (roomHumidityIsAvailable) {
        roomHumidity->seek(0);
        QByteArray line = roomHumidity->readLine();
        ui->humidity->setText((QString)line);
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
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

//    QString program = "/home/germana/Oeffentlich/Hausbus-git_working/tools/eventmonitor/bin/eventmonitor";
    QString program = "/root/eventmonitor";
    QStringList arguments;
    arguments << "-c" << "/dev/hausbus0" << "-a" << "250";

    eventState = eEsWaitForStart;
    eventMonitor->start(program, arguments);
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

    for (i = 0; i < ev_lines.size(); i++) {
        if (qstrcmp(ev_lines[i], "event address 240 device type DO31") == 0) {
           eventState = eEsWaitForDo240;
           continue;
        } else if (qstrcmp(ev_lines[i], "event address 241 device type DO31") == 0) {
           eventState = eEsWaitForDo241;
           continue;
        }
        switch (eventState) {
        case eEsWaitForStart:
            break;
        case eEsWaitForDo240:
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
            (ev_lines[i].at(14) == '0') ? io->egState.detail.lightGang = 0       : io->egState.detail.lightGang = 1;
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
            eventState = eEsWaitForSh240;
            break;
        case eEsWaitForDo241:
//            std::cout << "do 241: " << ev_lines[i].constData() << std::endl;
            (ev_lines[i].at(24) == '0') ? io->egState.detail.lightSpeis = 0      : io->egState.detail.lightSpeis = 1;
            (ev_lines[i].at(26) == '0') ? io->egState.detail.lightArbeit = 0     : io->egState.detail.lightArbeit = 1;
            (ev_lines[i].at(27) == '0') ? io->ogState.detail.lightAnna = 0       : io->ogState.detail.lightAnna = 1;
            (ev_lines[i].at(28) == '0') ? io->ogState.detail.lightSeverin = 0    : io->ogState.detail.lightSeverin = 1;
            (ev_lines[i].at(29) == '0') ? io->ogState.detail.lightWC = 0         : io->ogState.detail.lightWC = 1;
            (ev_lines[i].at(30) == '0') ? io->egState.detail.lightKuecheWand = 0 : io->egState.detail.lightKuecheWand = 1;
            eventState = eEsWaitForSh241;
            break;
        case eEsWaitForSh240:
//            std::cout << "sh 240: " << ev_lines[i].constData() << std::endl;
            eventState = eEsWaitForStart;
            break;
        case eEsWaitForSh241:
//            std::cout << "sh 241: " << ev_lines[i].constData() << std::endl;
            eventState = eEsWaitForStart;
            break;
        }
    }
//    ui->textBrowser->append(ev);

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
            ui->pushButtonGarage->setStyleSheet("background-color: yellow");
        }
        ui->pushButtonGarage->update();
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
