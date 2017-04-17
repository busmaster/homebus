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
    void serviceCmd(const char *);

private slots:
    void onIoStateChanged(void);

    void on_pushButtonDoorbell_pressed();
    void on_pushButtonInternet_clicked();

private:
    int do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen);
    Ui::setupwindow *ui;
    ioState *io;
    bool isVisible;
    bool doorbellState;
};

#endif // SETUPWINDOW_H
