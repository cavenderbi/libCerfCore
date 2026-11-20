#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/core/callback_list.h>

namespace cerfcore {

class Sh77xxSciPort : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual void SetTransmitter(bool enabled)                      = 0;
    virtual bool TxdPinLow() const                                 = 0;
    virtual void AddChangeListener(CallbackList<>::Callback listener) = 0;
};

}
