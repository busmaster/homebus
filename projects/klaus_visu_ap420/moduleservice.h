#ifndef MODULESERVICE_H
#define MODULESERVICE_H

#include <QObject>
#include <QProcess>
#include <QDialog>

class moduleservice : public QObject {
    Q_OBJECT
public:
    explicit moduleservice(QObject *parent = 0);
    bool command(const char *cmd);
    bool waitForFinished(void);

    enum cmdType {
        eSetvaldo31_do,
        eSetvalpwm4,
        eSwitchstate,
        eActval
    };

    struct cmd {
        quint8       destAddr;
        quint8       ownAddr;
        enum cmdType type;
        union {
            struct {
                quint8 setval[31];
            } setvaldo31_do;
            struct {
                quint8  set[4];
                quint16 pwm[4];
            } setvalpwm4;
            quint8 switchstate;
        } data;
    };
    bool command(const struct cmd *cmd, QDialog *dialog);

    enum resultType {
        eCmdState,
        eActvalState,
        eEventState
    };
    enum devType {
        eDevDo31,
        eDevSw8,
        eDevPwm4,
        eDevSmif
    };
    enum cmdState {
        eCmdOk,
        eCmdError
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
                quint16 pwm[4];
            } pwm4;
        } data;
    };

    struct actval {
        quint8       srcAddr;
        enum devType type;
        union {
            struct {
                quint32 a_plus;
                quint32 a_minus;
                quint32 r_plus;
                quint32 r_minus;
                quint32 p_plus;
                quint32 p_minus;
                quint32 q_plus;
                quint32 q_minus;
            } smif;
        } data;
    };

    struct result {
        enum resultType type;
        union {
            struct event  event;
            struct actval actval;
            enum cmdState state;
        } data;
    };
signals:
    void cmdConf(const struct moduleservice::result *, QDialog *);

private slots:
    void readStdOut();
    void onStarted();
    void onFinished(int sig);

private:
    QProcess *modulservice;
    enum {
        eEsWaitForStart,
        eEsWaitForDO31_Do,
        eEsWaitForDO31_Sh,
        eEsWaitForSW8,
        eEsWaitForPWM4
    } eventState;
    enum {
        eRsIdle,
        eRsWaitForState,
        eRsWaitForActval,
        eRsEvent
    } resultState;
    QDialog *resultDialog;

    struct result res;
};

#endif // MODULESERVICE_H
