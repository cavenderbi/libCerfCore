#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include "pxa270_id.h"
#include "pxa27x_i2c_device.h"
#include "pxa27x_i2c_unit.h"
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include "../irq_controller.h"

#include <cstdint>

namespace cerfcore {

namespace {

class Pxa27xI2c : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetSocId() == SocId::Pxa270;
    }
    void OnReady() override {
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            unit_.Reset(emu_.TryGet<Pxa27xI2cDevice>());
            UpdateIrq();
        });
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return 0x40300000u; }
    uint32_t MmioSize() const override { return 0x00002000u; }

    uint32_t ReadWord(uint32_t addr) override {
        uint32_t value = 0;
        if (!unit_.Read(UnitOffset(addr), value)) HaltUnsupportedAccess("ReadWord", addr, 0);
        return value;
    }

    void WriteWord(uint32_t addr, uint32_t value) override {
        if (!unit_.Write(UnitOffset(addr), value, emu_.TryGet<Pxa27xI2cDevice>()))
            HaltUnsupportedAccess("WriteWord", addr, value);
        UpdateIrq();
    }

    void SaveState(StateWriter& w) override {
        unit_.SaveState(w);
        if (auto* device = emu_.TryGet<Pxa27xI2cDevice>()) device->SaveState(w);
    }

    void RestoreState(StateReader& r) override {
        unit_.RestoreState(r);
        if (auto* device = emu_.TryGet<Pxa27xI2cDevice>()) device->RestoreState(r);
    }

    void PostRestore() override {
        if (auto* device = emu_.TryGet<Pxa27xI2cDevice>()) device->PostRestore();
        UpdateIrq();
    }

private:
    static constexpr uint32_t kIBMR        = 0x1680u;
    static constexpr uint32_t kAliasWindow = 0x2Cu;
    static constexpr uint32_t kNotARegister = 0xFFFFFFFFu;
    static constexpr int      kIntcI2c     = 18;

    uint32_t UnitOffset(uint32_t addr) const {
        const uint32_t off = addr - MmioBase();
        if (off < kAliasWindow) return off;
        return off >= kIBMR ? off - kIBMR : kNotARegister;
    }

    void UpdateIrq() {
        auto& intc = emu_.Get<IrqController>();
        if (unit_.InterruptActive()) intc.AssertIrq  (kIntcI2c);
        else                         intc.DeAssertIrq(kIntcI2c);
    }

    Pxa27xI2cUnit unit_;
};

}

REGISTER_SERVICE(Pxa27xI2c);

}
