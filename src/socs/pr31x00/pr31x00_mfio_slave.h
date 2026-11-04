#pragma once

#include <cerfcore/core/service.h>

#include "pr31x00_pins.h"

namespace cerfcore {

class StateWriter;
class StateReader;

class Pr31x00MfioSlave : public Service {
public:
    using Service::Service;

    virtual void OnMfioOut(const Pr31x00Pins& pins) = 0;

    virtual void SaveState(StateWriter&) {}
    virtual void RestoreState(StateReader&) {}
};

}
