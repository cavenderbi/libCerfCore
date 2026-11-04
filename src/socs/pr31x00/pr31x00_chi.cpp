#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include "pr31500_id.h"
#include "pr31700_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/state/state_stream.h>
#include "pr31x00_reset_line.h"

#include <cstdint>

namespace cerfcore {

namespace {

/* CHI Control Register, IFR offset $1D8 (TMPR3911 §7.4.1). */
constexpr uint32_t kBase = 0x10C001D8u;

constexpr uint32_t kEnChi   = 1u << 0;
constexpr uint32_t kChiTxEn = 1u << 1;
constexpr uint32_t kChiRxEn = 1u << 2;

constexpr uint32_t kCtlReset = 0u;

class Pr31x00Chi : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        const std::string_view soc = emu_.Get<BoardContext>().GetSocId();
        return soc == SocId::Pr31500 || soc == SocId::Pr31700;
    }
    void OnReady() override {
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<Pr31x00ResetLine>().RegisterResetListener([this] { ctl_ = kCtlReset; });
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return 0x4u; }

    uint32_t ReadWord(uint32_t addr) override {
        if (addr == kBase) return ctl_;
        HaltUnsupportedAccess("PR31x00 CHI ReadWord", addr, 0);
    }

    void WriteWord(uint32_t addr, uint32_t value) override {
        if (addr != kBase) HaltUnsupportedAccess("PR31x00 CHI WriteWord", addr, value);
        if (value & (kEnChi | kChiTxEn | kChiRxEn)) {
            HaltUnsupportedAccess("PR31x00 CHI enable (unmodelled codec highway)", addr, value);
        }
        ctl_ = value;
    }

    uint8_t  ReadByte(uint32_t addr) override { HaltUnsupportedAccess("PR31x00 CHI ReadByte", addr, 0); }
    uint16_t ReadHalf(uint32_t addr) override { HaltUnsupportedAccess("PR31x00 CHI ReadHalf", addr, 0); }
    void WriteByte(uint32_t addr, uint8_t  v) override { HaltUnsupportedAccess("PR31x00 CHI WriteByte", addr, v); }
    void WriteHalf(uint32_t addr, uint16_t v) override { HaltUnsupportedAccess("PR31x00 CHI WriteHalf", addr, v); }

    void SaveState(StateWriter& w) override { w.Write("ctl", ctl_); }
    void RestoreState(StateReader& r) override { r.Read("ctl", ctl_); }

private:
    uint32_t ctl_ = kCtlReset;
};

}  /* namespace */

REGISTER_SERVICE(Pr31x00Chi);

}
