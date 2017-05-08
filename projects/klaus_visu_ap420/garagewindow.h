#ifndef GARAGEWINDOW_H
#define GARAGEWINDOW_H

#include <stdint.h>
#include <QDialog>
#include "moduleservice.h"
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
    void serviceCmd(const moduleservice::cmd *, QDialog *);

private slots:
    void onCmdConf(const struct moduleservice::result *, QDialog *);
    void onIoStateChanged(void);

    void on_pushButtonLight_pressed();

private:
    Ui::garagewindow *ui;
    ioState *io;
    bool isVisible;

    QPushButton *currentButton;
    bool        currentButtonState;
};

#endif // GARAGEWINDOW_H
