#ifndef EGWINDOW_H
#define EGWINDOW_H

#include <stdint.h>
#include <QDialog>
#include "moduleservice.h"
#include "iostate.h"

namespace Ui {
class egwindow;
}

class egwindow : public QDialog {
    Q_OBJECT

public:
    explicit egwindow(QWidget *parent, ioState *state);
    ~egwindow();
    void show(void);
    void hide(void);

signals:
    void serviceCmd(const moduleservice::cmd *, QDialog *);

private slots:
    void onCmdConf(const struct moduleservice::result *, QDialog *);
    void onIoStateChanged(void);

    void on_pushButtonLightGang_pressed();
    void on_pushButtonLightWohn_pressed();
    void on_pushButtonLightEss_pressed();
    void on_pushButtonLightVorraum_pressed();
    void on_pushButtonLightArbeit_pressed();
    void on_pushButtonLightWC_pressed();
    void on_pushButtonLightTerrasse_pressed();
    void on_pushButtonLightWohnLese_pressed();

private:
    void sendDo31Cmd(quint8 destAddr, quint8 doNr, QPushButton *button, bool currState);
    Ui::egwindow *ui;
    ioState *io;
    bool isVisible;

    QPushButton *currentButton;
    bool        currentButtonState;
};

#endif // EGWINDOW_H
