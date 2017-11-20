#ifndef OGWINDOW_H
#define OGWINDOW_H

#include <stdint.h>
#include <QDialog>
#include "moduleservice.h"
#include "iostate.h"

namespace Ui {
class ogwindow;
}

class ogwindow : public QDialog {
    Q_OBJECT

public:
    explicit ogwindow(QWidget *parent, ioState *state);
    ~ogwindow();
    void show(void);
    void hide(void);

signals:
    void serviceCmd(const moduleservice::cmd *, QDialog *);

private slots:
    void onCmdConf(const struct moduleservice::result *, QDialog *);
    void onIoStateChanged(void);

    void on_pushButtonLightAnna_pressed();
    void on_pushButtonLightSeverin_pressed();
    void on_pushButtonLightWC_pressed();
    void on_pushButtonLightBad_pressed();
    void on_pushButtonLightBadSpiegel_pressed();
    void on_pushButtonLightVorraum_pressed();
    void on_pushButtonLightStiege_pressed();
    void on_pushButtonLightSchlaf_pressed();
    void on_pushButtonLightSchrank_pressed();
    void on_pushButtonBack_clicked();

private:
    void sendDo31Cmd(quint8 destAddr, quint8 doNr, QPushButton *button, bool currState);

    Ui::ogwindow *ui;
    ioState *io;
    bool isVisible;

    QPushButton *currentButton;
    bool        currentButtonState;
};

#endif // OGWINDOW_H
