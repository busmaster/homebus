#ifndef KUECHEWINDOW_H
#define KUECHEWINDOW_H

#include <stdint.h>
#include <QDialog>
#include <QMqttClient>
#include "iostate.h"

namespace Ui {
class kuechewindow;
}

class kuechewindow : public QDialog
{
    Q_OBJECT

public:
    explicit kuechewindow(QWidget *parent, ioState *state);
    ~kuechewindow();

    void show(void);
    void hide(void);

signals:
    void messagePublish(const char *, const char *);

private slots:
    void onIoStateChanged(void);

    void on_pushButtonLight_pressed();
    void on_pushButtonLightWand_pressed();
    void on_pushButtonLightAbwasch_pressed();
    void on_pushButtonLightSpeis_pressed();
    void on_pushButtonLightDunstabzug_pressed();
    void on_pushButtonLightKaffee_pressed();
    void on_pushButtonLightGeschirrspueler_pressed();
    void on_verticalSlider_valueChanged(int value);
    void on_pushButtonBack_clicked();

private:
    Ui::kuechewindow *ui;
    ioState *io;
    bool isVisible;
};

#endif // KUECHEWINDOW_H
