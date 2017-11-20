#ifndef KUECHEWINDOW_H
#define KUECHEWINDOW_H

#include <stdint.h>
#include <QDialog>
#include "moduleservice.h"
#include "iostate.h"

namespace Ui {
class kuechewindow;
}

class kuechewindow : public QDialog
{
    Q_OBJECT

public:
    explicit kuechewindow(QWidget *parent, ioState *state);
    ~kuechewindow();

    void show(void);
    void hide(void);

signals:
    void serviceCmd(const moduleservice::cmd *, QDialog *);

private slots:
    void onCmdConf(const struct moduleservice::result *, QDialog *);
    void onIoStateChanged(void);

    void on_pushButtonLight_pressed();
    void on_pushButtonLightWand_pressed();
    void on_pushButtonLightAbwasch_pressed();
    void on_pushButtonLightSpeis_pressed();
    void on_pushButtonLightDunstabzug_pressed();
    void on_pushButtonLightKaffee_pressed();
    void on_pushButtonLightGeschirrspueler_pressed();
    void on_verticalSlider_valueChanged(int value);
    void on_pushButtonBack_clicked();

private:
    void sendDo31Cmd(quint8 destAddr, quint8 doNr, QPushButton *button, bool currState);
    void sendPwm4Cmd(quint8 destAddr, quint8 pwmNr, QPushButton *button, bool currState);

    Ui::kuechewindow *ui;
    ioState *io;
    bool isVisible;

    QPushButton *currentButton;
    bool        currentButtonState;
};

#endif // KUECHEWINDOW_H
