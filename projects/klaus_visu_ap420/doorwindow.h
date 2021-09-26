#ifndef DOORWINDOW_H
#define DOORWINDOW_H

#include <stdint.h>
#include <QDialog>
#include <QMqttClient>
#include <QTimer>
#include "iostate.h"

namespace Ui {
class doorwindow;
}

class doorwindow : public QDialog {
    Q_OBJECT

public:
    explicit doorwindow(QWidget *parent, ioState *state);
    ~doorwindow();
    void show(void);
    void hide(void);

signals:
    void messagePublish(const char *, const char *);

private slots:
    void onIoStateChanged(void);

    void on_pushButtonRefresh_clicked();
    void on_pushButtonLock_pressed();
    void on_pushButtonOpen_pressed();
    void on_pushButtonBack_clicked();

    void openButtonTimerEvent();
    void lockButtonTimerEvent();
    void refreshButtonTimerEvent();

private:
    Ui::doorwindow *ui;
    ioState *io;
    bool isVisible;
    QTimer *openButtonTimer;
    QTimer *lockButtonTimer;
    QTimer *refreshButtonTimer;
};

#endif // DOORWINDOW_H
