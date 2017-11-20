#ifndef UGWINDOW_H
#define UGWINDOW_H

#include <stdint.h>
#include <QDialog>
#include "moduleservice.h"
#include "iostate.h"

namespace Ui {
class ugwindow;
}

class ugwindow : public QDialog {
    Q_OBJECT

public:
    explicit ugwindow(QWidget *parent, ioState *state);
    ~ugwindow();
    void show(void);
    void hide(void);

signals:
    void serviceCmd(const moduleservice::cmd *, QDialog *);

private slots:
    void onCmdConf(const struct moduleservice::result *, QDialog *);
    void onIoStateChanged(void);

    void on_pushButtonLightStiege_pressed();
    void on_pushButtonLightVorraum_pressed();
    void on_pushButtonLightTechnik_pressed();
    void on_pushButtonLightLager_pressed();
    void on_pushButtonLightFitness_pressed();
    void on_pushButtonLightArbeit_pressed();
    void on_pushButtonBack_clicked();

private:
    void sendDo31Cmd(quint8 destAddr, quint8 doNr, QPushButton *button, bool currState);

    Ui::ugwindow *ui;
    ioState *io;
    bool isVisible;

    QPushButton *currentButton;
    bool        currentButtonState;
};

#endif // UGWINDOW_H
