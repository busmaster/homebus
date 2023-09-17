#ifndef SMARTMETERWINDOW_H
#define SMARTMETERWINDOW_H

#include <stdint.h>
#include <QDialog>
#include <QTimer>
#include <QMqttClient>
#include "iostate.h"

namespace Ui {
class smartmeterwindow;
}

class smartmeterwindow : public QDialog {
    Q_OBJECT

public:
    explicit smartmeterwindow(QWidget *parent, ioState *state);
    ~smartmeterwindow();
    void show(void);
    void hide(void);

signals:
    void messagePublish(const char *, const char *);

private slots:
    void onMeterChanged(void);
    void onScreenSaverActivation(void);
    void on_pushButtonBack_clicked();

private:
    Ui::smartmeterwindow *ui;
    ioState *io;
    bool isVisible;
};

#endif // SMARTMETERWINDOW_H
