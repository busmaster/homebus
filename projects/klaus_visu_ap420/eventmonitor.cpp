#include <QProcess>
#include <iostream>

#include "eventmonitor.h"

eventmonitor::eventmonitor(QObject *parent) : QObject(parent) {

    eventmon = new QProcess;

    connect(eventmon, SIGNAL(readyRead()), this, SLOT(readStdOut()));
    connect(eventmon, SIGNAL(started()), this, SLOT(onStarted()));
    connect(eventmon, SIGNAL(finished(int)), this, SLOT(onFinished(int)));

    QString program = "/usr/bin/homebus/eventmonitor";
    QStringList arguments;
    arguments << "-c" << "/dev/hausbus0" << "-a" << "250" << "-l";

    evState = eEsWaitForStart;
    eventmon->start(program, arguments);
}

void eventmonitor::readStdOut() {

    int i;
    int j;
    bool doEmit = false;
    QByteArray output = eventmon->readAllStandardOutput();
    QList<QByteArray> ev_lines = output.split('\n');
    QList<QByteArray> field;

    for (i = 0; i < ev_lines.size(); i++) {
        if (qstrcmp(ev_lines[i], "event address 240 device type DO31") == 0) {
            evState = eEsWaitForDO31_Do;
            ev.srcAddr = 240;
            ev.type = eDevDo31;
            continue;
        } else if (qstrcmp(ev_lines[i], "event address 241 device type DO31") == 0) {
            evState = eEsWaitForDO31_Do;
            ev.srcAddr = 241;
            ev.type = eDevDo31;
            continue;
        } else if (qstrcmp(ev_lines[i], "event address 36 device type SW8") == 0) {
            evState = eEsWaitForSW8;
            ev.srcAddr = 36;
            ev.type = eDevSw8;
            continue;
        } else if (qstrcmp(ev_lines[i], "event address 30 device type SW8") == 0) {
            evState = eEsWaitForSW8;
            ev.srcAddr = 30;
            ev.type = eDevSw8;
            continue;
        } else if (qstrcmp(ev_lines[i], "event address 239 device type PWM4") == 0) {
            evState = eEsWaitForPWM4_State;
            ev.srcAddr = 239;
            ev.type = eDevPwm4;
            continue;
        }
        switch (evState) {
        case eEsWaitForStart:
            break;
        case eEsWaitForDO31_Do:
            for (j = 0; j < 31; j++) {
                if (ev_lines[i].at(j) == '1') {
                   ev.data.do31.digOut |= (1 << j);
                } else {
                   ev.data.do31.digOut &= ~(1 << j);
                }
            }
            evState = eEsWaitForDO31_Sh;
            break;
        case eEsWaitForDO31_Sh:
            evState = eEsWaitForStart;
            doEmit = true;
            break;
        case eEsWaitForSW8:
            for (j = 0; j < 8; j++) {
                if (ev_lines[i].at(j) == '1') {
                   ev.data.sw8.digInOut |= (1 << j);
                } else {
                   ev.data.sw8.digInOut &= ~(1 << j);
                }
            }
            evState = eEsWaitForStart;
            doEmit = true;
            break;
        case eEsWaitForPWM4_State:
            for (j = 0; j < 4; j++) {
                if (ev_lines[i].at(j) == '1') {
                   ev.data.pwm4.state |= (1 << j);
                } else {
                   ev.data.pwm4.state &= ~(1 << j);
                }
            }
            evState = eEsWaitForPWM4_Pwm;
            break;
        case eEsWaitForPWM4_Pwm:
            bool ok;
            field = ev_lines[i].split(' ');
            ev.data.pwm4.pwm[0] = field[0].toInt(&ok, 16);
            ev.data.pwm4.pwm[1] = field[1].toInt(&ok, 16);
            ev.data.pwm4.pwm[2] = field[2].toInt(&ok, 16);
            ev.data.pwm4.pwm[3] = field[3].toInt(&ok, 16);
            evState = eEsWaitForStart;
            doEmit = true;
            break;
        default:
            break;
        }

        if (doEmit) {
            emit busEvent(&ev);
            doEmit = false;
        }
    }
}

void eventmonitor::onStarted(){
    std::cout << "eventmonitor started" << std::endl;
}

void eventmonitor::onFinished(int sig) {
    std::cout << "eventmonitor finished: " << sig << std::endl;
}

bool eventmonitor::command(const char *cmd) {

    eventmon->write(cmd);
    eventmon->write("\n");

    return true;
}

bool eventmonitor::waitForFinished(void) {

    return eventmon->waitForFinished();
}



