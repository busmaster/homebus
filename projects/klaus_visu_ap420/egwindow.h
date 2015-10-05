#ifndef EGWINDOW_H
#define EGWINDOW_H

#include <QDialog>
#include <QProcess>
#include "iostate.h"

namespace Ui {
class egwindow;
}

class egwindow : public QDialog
{
    Q_OBJECT

public:
    explicit egwindow(QWidget *parent, ioState *state);
    ~egwindow();
    void show(void);
    void hide(void);

private slots:
    void onIoStateChanged(void);

    void on_pushButtonLightGang_pressed();
    void on_pushButtonLightWohn_pressed();
    void on_pushButtonLightKueche_pressed();
    void on_pushButtonLightEss_pressed();
    void on_pushButtonLightVorraum_pressed();
    void on_pushButtonLightKuecheWand_pressed();
    void on_pushButtonLightArbeit_pressed();
    void on_pushButtonLightSpeis_pressed();
    void on_pushButtonLightWC_pressed();
    void on_pushButtonLightTerrasse_pressed();

private:
    Ui::egwindow *ui;
    ioState *io;
    bool isVisible;
    QProcess *modulservice;
};

#endif // EGWINDOW_H
