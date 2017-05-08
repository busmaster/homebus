#ifndef IOSTATE_H
#define IOSTATE_H

#include <QObject>

class ioState : public QObject {
    Q_OBJECT
public:
    explicit ioState(QObject *parent = 0);

    union {
        quint32 sum;
        struct {
            int lightWohn       : 1;
            int lightEss        : 1;
            int lightGang       : 1;
            int lightWC         : 1;
            int lightVorraum    : 1;
            int lightArbeit     : 1;
            int lightTerrasse   : 1;
            int lightWohnLese   : 1;
        } detail;
    } egState;
    union {
        quint32 sum;
        struct {
            int lightStiegePwr  : 1;
            int lightStiege1    : 1;
            int lightStiege2    : 1;
            int lightStiege3    : 1;
            int lightStiege4    : 1;
            int lightStiege5    : 1;
            int lightStiege6    : 1;
            int lightSchrank    : 1;
            int lightSchlaf     : 1;
            int lightBad        : 1;
            int lightBadSpiegel : 1;
            int lightVorraum    : 1;
            int lightWC         : 1;
            int lightAnna       : 1;
            int lightSeverin    : 1;
        } detail;
    } ogState;
    union {
        quint32 sum;
        struct {
            int lightLager      : 1;
            int lightStiege     : 1;
            int lightArbeit     : 1;
            int lightFitness    : 1;
            int lightVorraum    : 1;
            int lightTechnik    : 1;
        } detail;
    } ugState;
    union {
        quint32 sum;
        struct {
            int light           : 1;
            int door            : 1;
        } detail;
    } garageState;

    union {
        quint32 sum;
        struct {
            int light                : 1;
            int lightWand            : 1;
            int lightKaffee          : 1;
            int lightGeschirrspueler : 1;
            int lightDunstabzug      : 1;
            int lightAbwasch         : 1;
            int lightSpeis           : 1;
        } detail;
    } kuecheState;

    /* switchable sockets Technik */
    bool socket_1;
    bool socket_2;


    struct {
        quint32 countA_plus;
        quint32 countA_minus;
        quint32 countR_plus;
        quint32 countR_minus;
        quint32 activePower_plus;
        quint32 activePower_minus;
        quint32 reactivePower_plus;
        quint32 reactivePower_minus;
    } smartmeter;
};

#endif // IOSTATE_H
