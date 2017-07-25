#ifndef EVENTMONITOR_H
#define EVENTMONITOR_H

#include <QObject>
#include <QProcess>

class eventmonitor : public QObject {
    Q_OBJECT
public:
    explicit eventmonitor(QObject *parent = 0);
    bool command(const char *cmd);
    bool waitForFinished(void);

    enum devType {
        eDevDo31,
        eDevSw8,
        eDevPwm4
    };

    struct event {
        quint8       srcAddr;
        enum devType type;
        union {
            struct {
                quint32 digOut;
                quint8  sh[15];
            } do31;
            struct {
                quint8  digInOut;
            } sw8;
            struct {
                quint8  state;
                quint16 pwm[4];
            } pwm4;
        } data;
    };

signals:
    void busEvent(struct eventmonitor::event *);

private slots:
    void readStdOut();
    void onStarted();
    void onFinished(int sig);

private:
    QProcess *eventmon;
    enum {
        eEsWaitForStart,
        eEsWaitForDO31_Do,
        eEsWaitForDO31_Sh,
        eEsWaitForSW8,
        eEsWaitForPWM4_State,
        eEsWaitForPWM4_Pwm,
    } evState;

    struct eventmonitor::event ev;
};

#endif // EVENTMONITOR_H
