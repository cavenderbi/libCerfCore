#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include "../../boards/casio_cassiopeia_a10/casio_cassiopeia_a10_family_id.h"
#include "../../boards/casio_pocketpostpet/casio_pocketpostpet_family_id.h"
#include "../../boards/casio_tomcat/casio_tomcat_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include "../peripheral_dispatcher.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>

#include <algorithm>
#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x10000120u;
constexpr uint32_t kSize = 0x08u;

constexpr uint32_t kOffData  = 0x00u;
constexpr uint32_t kOffCtrl  = 0x02u;
constexpr uint32_t kOffProbe = 0x04u;

constexpr uint16_t kCtrlPresent = 0x2320u;

constexpr uint16_t kDataBits      = 0x00FFu;
constexpr uint16_t kCtrlValues[]  = {0x1100u, 0x3330u, 0x9100u, 0x9900u};
constexpr uint16_t kProbeValues[] = {0x0000u, 0x0080u};

class CasioHostLink : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        if (!bd) return false;
        const auto b = bd->GetBoardFamilyId();
        return b == BoardFamilyId::CasioPocketpostpet || b == BoardFamilyId::CasioTomcat ||
               b == BoardFamilyId::CasioCassiopeiaA10;
    }
    void OnReady() override {
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { probe_ = 0u; });
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    uint16_t ReadHalf(uint32_t addr) override {
        switch (addr - kBase) {
            case kOffCtrl:  return kCtrlPresent;
            case kOffProbe: return probe_;
            default: HaltUnsupportedAccess("CasioHostLink ReadHalf", addr, 0);
        }
    }
    void WriteHalf(uint32_t addr, uint16_t value) override {
        switch (addr - kBase) {
            case kOffData:
                if ((value & ~kDataBits) != 0u) break;
                LOG(Board, "Casio host-link TX 0x%02X (no docked peer)\n", value);
                return;
            case kOffCtrl:
                if (std::ranges::find(kCtrlValues, value) == std::ranges::end(kCtrlValues)) break;
                return;
            case kOffProbe:
                if (std::ranges::find(kProbeValues, value) == std::ranges::end(kProbeValues)) break;
                probe_ = value;
                return;
            default:
                break;
        }
        HaltUnsupportedAccess("CasioHostLink WriteHalf", addr, value);
    }

    uint8_t  ReadByte (uint32_t addr) override { HaltUnsupportedAccess("CasioHostLink ReadByte", addr, 0); }
    uint32_t ReadWord (uint32_t addr) override { HaltUnsupportedAccess("CasioHostLink ReadWord", addr, 0); }
    void WriteByte(uint32_t addr, uint8_t  v) override { HaltUnsupportedAccess("CasioHostLink WriteByte", addr, v); }
    void WriteWord(uint32_t addr, uint32_t v) override { HaltUnsupportedAccess("CasioHostLink WriteWord", addr, v); }

    void SaveState(StateWriter& w) override { w.Write("probe", probe_); }
    void RestoreState(StateReader& r) override { r.Read("probe", probe_); }

private:
    uint16_t probe_ = 0u;
};

}

REGISTER_SERVICE(CasioHostLink);

}
