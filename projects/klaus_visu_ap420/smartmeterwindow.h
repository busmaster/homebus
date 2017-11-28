#ifndef SMARTMETERWINDOW_H
#define SMARTMETERWINDOW_H

#include <stdint.h>
#include <QDialog>
#include <QTimer>
#include "moduleservice.h"

namespace Ui {
class smartmeterwindow;
}

class smartmeterwindow : public QDialog {
    Q_OBJECT

public:
    explicit smartmeterwindow(QWidget *parent);
    ~smartmeterwindow();
    void show(void);
    void hide(void);

signals:
    void serviceCmd(const char *);
    void serviceCmd(const moduleservice::cmd *, QDialog *);

private slots:
    void onCmdConf(const struct moduleservice::result *, QDialog *);
    void onScreenSaverActivation(void);
    void updTimerEvent(void);
    void on_pushButtonBack_clicked();

private:
    Ui::smartmeterwindow *ui;
    bool isVisible;
    QTimer *updTimer;
};

#endif // SMARTMETERWINDOW_H
