#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>
#include <functional>

namespace cerfcore {

class Vr41xxAiuDma : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual void     SyncDma()     = 0;
    virtual uint16_t DmaRequests() = 0;

    void WriteDmaRegister(const std::function<void()>& write);

protected:
    virtual void DmaRegistersWritten() = 0;
};

}
