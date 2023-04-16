#ifndef IOSTATE_H
#define IOSTATE_H

#include <QObject>


class ioState : public QObject {
    Q_OBJECT
public:
    explicit ioState(QObject *parent = 0);

    enum egLightBits {
        egLightWohn         = 0x1,
        egLightEss          = 0x2,
        egLightGang         = 0x4,
        egLightWC           = 0x8,
        egLightVorraum      = 0x10,
        egLightArbeit       = 0x20,
        egLightSchreibtisch = 0x40,
        egLightTerrasse     = 0x80,
        egLightWohnLese     = 0x100,
        egLightEingang      = 0x200
    };
    quint32 egLight;

    enum ogLightBits {
        ogLightStiegePwr  = 0x1,
        ogLightStiege1    = 0x2,
        ogLightStiege2    = 0x4,
        ogLightStiege3    = 0x8,
        ogLightStiege4    = 0x10,
        ogLightStiege5    = 0x20,
        ogLightStiege6    = 0x40,
        ogLightSchrank    = 0x80,
        ogLightSchlaf     = 0x100,
        ogLightBad        = 0x200,
        ogLightBadSpiegel = 0x400,
        ogLightVorraum    = 0x800,
        ogLightWC         = 0x1000,
        ogLightAnna       = 0x2000,
        ogLightSeverin    = 0x4000
    };
    quint32 ogLight;

    enum ugLightBits {
        ugLightLager      = 0x1,
        ugLightStiege     = 0x2,
        ugLightArbeit     = 0x4,
        ugLightFitness    = 0x8,
        ugLightVorraum    = 0x10,
        ugLightTechnik    = 0x20
    };
    quint32 ugLight;


    enum garageBits {
        garageLight       = 0x1,
        garageDoorClosed  = 0x2,
    };
    quint32 garage;

    enum kuecheLightBits {
        kuecheLight                = 0x1,
        kuecheLightWand            = 0x2,
        kuecheLightKaffee          = 0x4,
        kuecheLightGeschirrspueler = 0x8,
        kuecheLightDunstabzug      = 0x10,
        kuecheLightAbwasch         = 0x20,
        kuecheLightSpeis           = 0x40,
    };
    quint32 kueche;

    /* switchable sockets Technik */
    enum socketBits {
        socketInternet = 0x1,
        socketCamera   = 0x2
    };
    quint32 sockets;

    quint32 glocke_taster;
    quint8  var_glocke_disable;
    quint8  var_mode_LightEingang;

    struct smartmeter {
        quint32 a_plus;
        quint32 a_minus;
        quint32 r_plus;
        quint32 r_minus;
        quint32 p_plus;
        quint32 p_minus;
        quint32 q_plus;
        quint32 q_minus;
        quint32 a_plus_midnight;
        quint32 a_minus_midnight;
    } sm;

    struct solar {
        quint32 oben_ost;
        quint32 oben_mitte;
        quint32 oben_west;
        quint32 unten_ost;
        quint32 unten_mitte;
        quint32 unten_west;
    } solar;

    enum doorState {
        locked,
        unlocked,
        internal,
        invalid1,
        invalid2,
        noresp,
        noconnection,
        uncalib,
        again
    };
    struct door {
        enum doorState lockstate;
    } door;
};

#endif // IOSTATE_H
