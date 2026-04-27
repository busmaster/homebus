#include "iostate.h"

ioState::ioState(QObject *parent) : QObject(parent) {
    egLight = 0;
    ogLight = 0;
    ugLight = 0;
    garage = 0;
    kueche = 0;
    sockets = 0;
    glocke_taster = 0;
    var_glocke_disable = 0;
    var_mode_LightEingang = 0;
    memset(&storage, 0, sizeof(storage));
    memset(&sp, 0, sizeof(sp));
    memset(&sm, 0, sizeof(sm));
    memset(&sy, 0, sizeof(sy));
    memset(&door, 0, sizeof(door));
}

