#ifndef IOSTATE_H
#define IOSTATE_H

#include <QObject>

class ioState : public QObject
{
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
            int lightKueche     : 1;
            int lightSpeis      : 1;
            int lightKuecheWand : 1;
            int lightArbeit     : 1;
            int lightTerrasse   : 1;
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
        } detail;
    } garageState;
};

#endif // IOSTATE_H
