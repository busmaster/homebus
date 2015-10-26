#ifndef GARAGEWINDOW_H
#define GARAGEWINDOW_H

#include <stdint.h>
#include <QDialog>
#include "iostate.h"

namespace Ui {
class garagewindow;
}

class garagewindow : public QDialog {
    Q_OBJECT

public:
    explicit garagewindow(QWidget *parent, ioState *state);
    ~garagewindow();
    void show(void);
    void hide(void);

signals:
    void serviceCmd(const char *);

private slots:
    void onIoStateChanged(void);

    void on_pushButtonLight_pressed();

private:
    int do31Cmd(int do31Addr, uint8_t *pDoState, size_t stateLen, char *pCmd, size_t cmdLen);
    Ui::garagewindow *ui;
    ioState *io;
    bool isVisible;
};

#endif // GARAGEWINDOW_H
