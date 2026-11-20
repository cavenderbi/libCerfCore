#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "sh77xx_bsc.h"

#include <cstdint>

namespace cerfcore {

namespace {

class Sh77xxSdramModeRegister : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        const Sh77xxBsc* bsc = emu_.TryGet<Sh77xxBsc>();
        return bsc != nullptr && bsc->HasSdramModeRegister();
    }

    void OnReady() override { emu_.Get<PeripheralDispatcher>().Register(this); }

    uint32_t MmioBase() const override { return kArea2Window; }
    uint32_t MmioSize() const override { return kWindowBytes * 2u; }

    void WriteByte(uint32_t, uint8_t) override {}
    void WriteWord(uint32_t, uint32_t) override {}

private:
    static constexpr uint32_t kArea2Window = 0x1FFFD000u;
    static constexpr uint32_t kWindowBytes = 0x1000u;
};

}

REGISTER_SERVICE(Sh77xxSdramModeRegister);

}
