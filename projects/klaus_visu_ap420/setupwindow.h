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

    void on_pushButtonGlockeAus_pressed();
    void on_pushButtonGlockeEin_pressed();
    void on_pushButtonInternetAus_pressed();
    void on_pushButtonInternetEin_pressed();
    void on_pushButtonLichtEingangEin_pressed();
    void on_pushButtonLichtEingangAus_pressed();
    void on_pushButtonLichtEingangAuto_pressed();

private:
    Ui::setupwindow *ui;
    ioState *io;
    bool isVisible;

    enum {
        eCurrNone,
        eCurrInternetEin,
        eCurrInternetAus,
        eCurrGlockeEin,
        eCurrGlockeAus,
        eCurrLichtEingangAuto,
        eCurrLichtEingangEin,
        eCurrLichtEingangAus
    } currentButton;
};

#endif // SETUPWINDOW_H
