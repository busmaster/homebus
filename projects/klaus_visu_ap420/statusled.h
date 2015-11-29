#ifndef STATUSLED_H
#define STATUSLED_H

#include <QObject>
#include <QFile>

class statusled : public QObject
{
    Q_OBJECT
public:
    explicit statusled(QObject *parent = 0);
    ~statusled();

    enum ledstate {
        eOff,
        eGreen,
        eGreenBlink,
        eOrange,
        eOrangeBlink,
        eRed,
        eRedBlink
    };

    void set_state(enum ledstate state);
    void get_state(enum ledstate *state);
signals:

public slots:

private:
    bool check_device(const char *device);
    void set_brightness(const char *device, const char *brightness);
    void set_trigger(const char *device, const char *trigger);
    void set_delay(const char *device, const char *onOff, const char *delay);
    void set_blink(const char *device, const char *onDelay, const char *offDelay);

    QFile *file;
    bool ledIsAvailable;

    enum ledstate state;

    const char *led_red;
    const char *led_green;
};

#endif // STATUSLED_H
