#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <iostream>
#include <QTimer>
#include <QFile>
#include <QString>
#include <QMqttClient>

#include "egwindow.h"
#include "ogwindow.h"
#include "ugwindow.h"
#include "kuechewindow.h"
#include "garagewindow.h"
#include "setupwindow.h"
#include "smartmeterwindow.h"
#include "kameraeingangwindow.h"
#include "doorwindow.h"
#include "iostate.h"
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
    void meterChanged(void);
    void doorChanged(void);
    void screenSaverActivated(void);

public slots:
    void onDisableScreenSaver(void);
    void onMessagePublish(const char *, const char *);

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
    void on_pushButtonKameraEingang_clicked();
    void on_pushButtonDoor_clicked();
    void onMqtt_connected();
    void onMqtt_disconnected();
    void onMqtt_messageReceived(const QByteArray &, const QMqttTopicName &);
    void mqttReconnectTimerEvent();

private:
    void messageActionStateBit(const QByteArray &, quint32 *, quint32);
    void messageActionVar(const QByteArray &, quint8 *, quint8);
    void message_to_byteArray(const QString &, QByteArray &);
    void mqtt_subscribe(const QString &, int index);
    bool get_hwmon_dev(const QString &, QString &);
    Ui::MainWindow *ui;
    egwindow *uiEg;
    ogwindow *uiOg;
    ugwindow *uiUg;
    kuechewindow *uiKueche;
    garagewindow *uiGarage;
    setupwindow *uiSetup;
    smartmeterwindow *uiSmartmeter;
    kameraeingangwindow *uiKameraeingang;
    doorwindow *uiDoor;

    QTimer *scrTimer;
    QTimer *cycTimer;
    bool screensaverOn;

    QFile *backlightBrightness;
    QFile *fbBlank;

    QFile *roomTemperature;
    QString *roomTemperatureStr;

    QFile *roomHumidity;
    QString *roomHumidityStr;

    ioState *io;

    QMqttClient *mqttClient;
    QTimer *mqttReconnectTimer;
    QHash<QString, int> topic_hash;
};

#endif // MAINWINDOW_H
