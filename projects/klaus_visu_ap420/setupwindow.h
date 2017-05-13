#ifndef SETUPWINDOW_H
#define SETUPWINDOW_H

#include <stdint.h>
#include <QDialog>
#include "moduleservice.h"
#include "iostate.h"

namespace Ui {
class setupwindow;
}

class setupwindow : public QDialog {
    Q_OBJECT

public:
    explicit setupwindow(QWidget *parent, ioState *state);
    ~setupwindow();
    void show(void);
    void hide(void);

signals:
    void serviceCmd(const moduleservice::cmd *, QDialog *);

private slots:
    void onCmdConf(const struct moduleservice::result *, QDialog *);
    void onIoStateChanged(void);

    void on_pushButtonDoorbell_pressed();
    void on_pushButtonInternet_clicked();

private:
    Ui::setupwindow *ui;
    ioState *io;
    bool isVisible;
    bool doorbellState;

    QPushButton *currentButton;
    bool        currentButtonState;
};

#endif // SETUPWINDOW_H
