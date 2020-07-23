#include <QFile>
#include "statusled.h"

statusled::statusled(QObject *parent) : QObject(parent) {

    file = new QFile();
    if (!file) {
        return;
    }

    led_red =   "/sys/class/leds/red";
    led_green = "/sys/class/leds/green";

    if (!check_device(led_green) ||
        !check_device(led_red)) {
        delete file;
        file = 0;
        ledIsAvailable = false;
        return;
    }

    ledIsAvailable = true;
    state = eOrange;
    set_state(eOff);
}

statusled::~statusled() {

    if (file) {
        delete file;
    }
}

bool statusled::check_device(const char *device) {

    char name[100];
    char line[100];
    bool rc = false;

    snprintf(name, sizeof(name), "%s/brightness", device);
    file->setFileName(name);
    if (!file->exists()) {
        return false;
    }
    snprintf(name, sizeof(name), "%s/trigger", device);
    file->setFileName(name);
    if (!file->exists()) {
        return false;
    }
    if (file->open(QIODevice::ReadWrite)) {
        if (file->read(line, sizeof(line)) > 0) {
            if ((strstr(line, "timer") != 0) &&
                (strstr(line, "none") != 0)) {
                rc = true;
            }
        }
        file->close();
    }

    return rc;
}

void statusled::set_brightness(const char *device, const char *brightness) {

    char name[100];

    snprintf(name, sizeof(name), "%s/brightness", device);
    file->setFileName(name);
    if (file->open(QIODevice::ReadWrite)) {
        file->write(brightness);
        file->close();
    }
}


void statusled::set_trigger(const char *device, const char *trigger) {

    char name[100];

    snprintf(name, sizeof(name), "%s/trigger", device);
    file->setFileName(name);
    if (file->open(QIODevice::ReadWrite)) {
        file->write(trigger);
        file->close();
    }
}

void statusled::set_delay(const char *device, const char *onOff, const char *delay) {

    char name[100];

    snprintf(name, sizeof(name), "%s/%s", device, onOff);
    file->setFileName(name);
    if (file->open(QIODevice::ReadWrite)) {
        file->write(delay);
        file->close();
    }
}

void statusled::set_blink(const char *device, const char *onDelay, const char *offDelay) {

    if (!onDelay || !offDelay) {
        set_trigger(device, "none");
    } else {
        set_trigger(device, "timer");
        set_delay(device, "on", onDelay);
        set_delay(device, "off", offDelay);
    }
}

void statusled::set_state(enum ledstate new_state) {

    if (!ledIsAvailable) {
        return;
    }

    if (new_state == state) {
        return;
    }
    state = new_state;

    switch (new_state) {
    case eOff:
        set_blink(led_green, 0, 0);
        set_brightness(led_green, "0");
        set_blink(led_red, 0, 0);
        set_brightness(led_red, "0");
        break;
    case eGreen:
        set_blink(led_green, 0, 0);
        set_brightness(led_green, "1");
        set_blink(led_red, 0, 0);
        set_brightness(led_red, "0");
        break;
    case eGreenBlink:
        set_blink(led_green, "200", "200");
        set_blink(led_red, 0, 0);
        set_brightness(led_red, "0");
        break;
    case eOrange:
        set_blink(led_green, 0, 0);
        set_brightness(led_green, "1");
        set_blink(led_red, 0, 0);
        set_brightness(led_red, "1");
        break;
    case eOrangeBlink:
        set_blink(led_green, "200", "200");
        set_blink(led_red, "200", "200");
        break;
    case eRed:
        set_blink(led_green, 0, 0);
        set_brightness(led_green, "0");
        set_blink(led_red, 0, 0);
        set_brightness(led_red, "1");
        break;
    case eRedBlink:
        set_blink(led_green, 0, 0);
        set_brightness(led_green, "0");
        set_blink(led_red, "200", "200");
        break;
    default:
        break;
    }
}
