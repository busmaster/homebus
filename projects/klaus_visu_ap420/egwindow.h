#ifndef EGWINDOW_H
#define EGWINDOW_H

#include <stdint.h>
#include <QDialog>
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
    void serviceCmd(const char *);

private slots:
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
    int do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen);
    Ui::egwindow *ui;
    ioState *io;
    bool isVisible;
};

#endif // EGWINDOW_H
