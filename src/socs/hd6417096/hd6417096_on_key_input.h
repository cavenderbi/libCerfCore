#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

namespace cerfcore {

class Hd6417096OnKeyInput : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual void SetOnKeyHeld(bool held, bool cpu_running) = 0;
};

}
