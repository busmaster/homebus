#ifndef UGWINDOW_H
#define UGWINDOW_H

#include <stdint.h>
#include <QDialog>
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
    void serviceCmd(const char *);

private slots:
    void onIoStateChanged(void);

    void on_pushButtonLightStiege_pressed();
    void on_pushButtonLightVorraum_pressed();
    void on_pushButtonLightTechnik_pressed();
    void on_pushButtonLightLager_pressed();
    void on_pushButtonLightFitness_pressed();
    void on_pushButtonLightArbeit_pressed();

private:
    int do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen);
    Ui::ugwindow *ui;
    ioState *io;
    bool isVisible;
};

#endif // UGWINDOW_H
