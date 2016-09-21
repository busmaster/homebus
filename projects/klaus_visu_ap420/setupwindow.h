#ifndef SETUP_H
#define SETUP_H

#include <stdint.h>
#include <QDialog>
#include "iostate.h"

namespace Ui {
class setupwindow;
}

class setupwindow : public QDialog
{
    Q_OBJECT

public:
    explicit setupwindow(QWidget *parent, ioState *state);
    ~setupwindow();
    void show(void);
    void hide(void);

signals:
    void serviceCmd(const char *);

private slots:
    void on_pushButtonDoorbell_pressed();

private:
    Ui::setupwindow *ui;
    bool isVisible;
    bool doorbellState;
};

#endif // SETUP_H
