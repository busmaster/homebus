#include "iostate.h"

ioState::ioState(QObject *parent) : QObject(parent) {
   egState.sum = 0;
   ogState.sum = 0;
   ugState.sum = 0;
   garageState.sum = 0;
}

