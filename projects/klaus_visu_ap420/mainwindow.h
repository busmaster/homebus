#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <iostream>
#include <QTimer>
#include <QFile>
#include <QString>

#include "egwindow.h"
#include "ogwindow.h"
#include "ugwindow.h"
#include "kuechewindow.h"
#include "garagewindow.h"
#include "setupwindow.h"
#include "smartmeterwindow.h"
#include "iostate.h"
#include "moduleservice.h"
#include "eventmonitor.h"
#include "statusled.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    bool eventFilter(QObject *obj, QEvent *event);

    statusled *statusLed;

signals:
    void ioChanged(void);
    void cmdConf(const struct moduleservice::result *, QDialog *);

public slots:
    void onSendServiceCmd(const struct moduleservice::cmd *, QDialog *);

private slots:
    void scrTimerEvent();
    void cycTimerEvent();
    void on_pushButtonEG_clicked();
    void on_pushButtonOG_clicked();
    void on_pushButtonUG_clicked();
    void on_pushButtonKueche_clicked();
    void on_pushButtonGarage_clicked();
    void on_pushButtonSetup_clicked();
    void on_pushButtonSmartMeter_clicked();

    void onBusEvent(struct eventmonitor::event *);
    void onCmdConf(const struct moduleservice::result *, QDialog *);


private:
    Ui::MainWindow *ui;
    egwindow *uiEg;
    ogwindow *uiOg;
    ugwindow *uiUg;
    kuechewindow *uiKueche;
    garagewindow *uiGarage;
    setupwindow *uiSetup;
    smartmeterwindow *uiSmartmeter;

    QTimer *scrTimer;
    QTimer *cycTimer;
    bool screensaverOn;

    QFile *backlightBrightness;
    bool backlightIsAvailable;

    QFile *fbBlank;
    bool fbBlankIsAvailable;

    QFile *roomTemperature;
    bool roomTemperatureIsAvailable;
    QString *roomTemperatureStr;

    QFile *roomHumidity;
    bool roomHumidityIsAvailable;
    QString *roomHumidityStr;

    ioState *io;

    moduleservice *mservice;
    eventmonitor *meventmon;
};

#endif // MAINWINDOW_H
