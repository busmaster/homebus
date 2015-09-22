#ifndef OGWINDOW_H
#define OGWINDOW_H

#include <QDialog>
#include <QProcess>
#include "iostate.h"

namespace Ui {
class ogwindow;
}

class ogwindow : public QDialog
{
    Q_OBJECT

public:
    explicit ogwindow(QWidget *parent, ioState *state);
    ~ogwindow();
    void show(void);
    void hide(void);

private slots:
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

private:
    Ui::ogwindow *ui;
    ioState *io;
    bool isVisible;
    QProcess *modulservice;
};

#endif // OGWINDOW_H
