#include "../vr41xx/vr41xx_scan_rep_kiu.h"

#include <cerfcore/boards/board_context.h>
#include "vr4121_id.h"
#include <cerfcore/core/cerf_emulator.h>

#include <cstdint>

namespace cerfcore {

namespace {

constexpr Vr41xxKiuModel kModel = {
    .stprep_zero_count        = 0u,
    .scanstart_auto_clear     = true,
    .keyen_scanline_interlock = true,
    .keyen_stop_deferred      = true,
    .scanstart_while_running_defined = true,
    .scanstp_completes_when_idle     = false,
    .gpen_retained_on_other_reset = true,
    .gpen_survives_kiurst         = true,
    .scanline_none_drives_all     = true,
    .scan_pins_drive_piod         = true,
    .count_bias                   = 1u,
    .zero_count_rfu               = true,
};

class Vr4121Kiu : public Vr41xxScanRepKiu {
public:
    using Vr41xxScanRepKiu::Vr41xxScanRepKiu;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4121;
    }

protected:
    const Vr41xxKiuModel& Model() const override { return kModel; }
};

}

REGISTER_SERVICE_AS(Vr4121Kiu, Vr41xxKiu);

}
