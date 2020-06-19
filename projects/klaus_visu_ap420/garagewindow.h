#ifndef GARAGEWINDOW_H
#define GARAGEWINDOW_H

#include <stdint.h>
#include <QDialog>
#include <QMqttClient>
#include "iostate.h"

namespace Ui {
class garagewindow;
}

class garagewindow : public QDialog {
    Q_OBJECT

public:
    explicit garagewindow(QWidget *parent, ioState *state);
    ~garagewindow();
    void show(void);
    void hide(void);

signals:
    void messagePublish(const char *, const char *);

private slots:
    void onIoStateChanged(void);

    void on_pushButtonLight_pressed();
    void on_pushButtonBack_clicked();

private:
    Ui::garagewindow *ui;
    ioState *io;
    bool isVisible;
};

#endif // GARAGEWINDOW_H
