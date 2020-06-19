#ifndef SETUPWINDOW_H
#define SETUPWINDOW_H

#include <stdint.h>
#include <QDialog>
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
    void messagePublish(const char *, const char *);

private slots:
    void onIoStateChanged(void);

    void on_pushButtonGlockeAus_pressed();
    void on_pushButtonGlockeEin_pressed();
    void on_pushButtonInternetAus_pressed();
    void on_pushButtonInternetEin_pressed();
    void on_pushButtonLichtEingangEin_pressed();
    void on_pushButtonLichtEingangAus_pressed();
    void on_pushButtonLichtEingangAuto_pressed();
    void on_pushButtonBack_clicked();

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
