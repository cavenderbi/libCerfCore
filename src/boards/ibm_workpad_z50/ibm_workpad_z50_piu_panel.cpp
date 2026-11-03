#include "../../socs/vr41xx/vr41xx_piu_panel.h"
#include "../../socs/vr41xx/vr41xx_piu_regs.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "ibm_workpad_z50_battery.h"
#include "ibm_workpad_z50_family_id.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

namespace {

using cerf_vr41xx_piu_detail::kAdcmdAdin0;
using cerf_vr41xx_piu_detail::kAdcmdAdin1;
using cerf_vr41xx_piu_detail::kAdcmdAdin2;
using cerf_vr41xx_piu_detail::kAdcmdAudioIn;

constexpr uint32_t kMainFullChargeCount = 944u;
constexpr uint16_t kBackupHealthy      = 0x316u;
constexpr uint16_t kChargeCompleteLow  = 0x0FFu;
constexpr uint16_t kChargeIncomplete   = 0x100u;
constexpr uint16_t kAudioInSample      = 0x000u;

class IbmWorkpadZ50PiuPanel : public Vr41xxPiuPanel {
public:
    using Vr41xxPiuPanel::Vr41xxPiuPanel;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
    }

    bool HasTouchscreen() const override { return false; }

    std::optional<uint16_t> ConvertCommandPort(uint16_t adcmd, uint16_t, uint16_t) override {
        emu_.Get<Fatal>().Die("IbmWorkpadZ50PiuPanel: PIU command scan of A/D port 0x%X", adcmd);
    }

    std::optional<uint16_t> PressureSample() override {
        emu_.Get<Fatal>().Die("IbmWorkpadZ50PiuPanel: PIU pressure sample");
    }

    std::optional<uint16_t> AdPortScanSample(uint16_t port) override {
        const auto& battery = emu_.Get<IbmWorkpadZ50Battery>();
        switch (port) {
            case kAdcmdAdin0:
                return static_cast<uint16_t>(battery.VoltageMv() * kMainFullChargeCount /
                                             battery.FullChargeMv());
            case kAdcmdAdin1: return kBackupHealthy;
            case kAdcmdAdin2: return battery.ChargeComplete() ? kChargeCompleteLow : kChargeIncomplete;
            case kAdcmdAudioIn: return kAudioInSample;
            default:
                emu_.Get<Fatal>().Die("IbmWorkpadZ50PiuPanel: ADPortScan of A/D port 0x%X", port);
        }
    }
};

}

REGISTER_SERVICE_AS(IbmWorkpadZ50PiuPanel, Vr41xxPiuPanel);

}
