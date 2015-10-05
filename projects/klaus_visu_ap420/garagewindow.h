#ifndef GARAGEWINDOW_H
#define GARAGEWINDOW_H

#include <QDialog>
#include <QProcess>
#include "iostate.h"

namespace Ui {
class garagewindow;
}

class garagewindow : public QDialog
{
    Q_OBJECT

public:
    explicit garagewindow(QWidget *parent, ioState *state);
    ~garagewindow();
    void show(void);
    void hide(void);

private slots:
    void onIoStateChanged(void);

    void on_pushButtonLight_pressed();

private:
    Ui::garagewindow *ui;
    ioState *io;
    bool isVisible;
    QProcess *modulservice;
};

#endif // GARAGEWINDOW_H
