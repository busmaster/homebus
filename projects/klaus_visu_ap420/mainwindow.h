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
#include "garagewindow.h"
#include "iostate.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    bool eventFilter(QObject *obj, QEvent *event);
    void InitEventMonitor(void);

signals:
    void ioChanged(void);

private slots:
    void readStdOut();
    void onStarted();
    void onFinished(int);
    void scrTimerEvent();
    void cycTimerEvent();
    void on_pushButtonEG_clicked();
    void on_pushButtonOG_clicked();
    void on_pushButtonUG_clicked();
    void on_pushButtonGarage_clicked();

private:
    Ui::MainWindow *ui;
    egwindow *uiEg;
    ogwindow *uiOg;
    ugwindow *uiUg;
    garagewindow *uiGarage;

    QProcess *eventMonitor;
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

    enum {
        eEsWaitForStart,
        eEsWaitForDO31_240_Do,
        eEsWaitForDO31_241_Do,
        eEsWaitForDO31_240_Sh,
        eEsWaitForDO31_241_Sh,
        eEsWaitForSW8_1
    } eventState;
};

#endif // MAINWINDOW_H
