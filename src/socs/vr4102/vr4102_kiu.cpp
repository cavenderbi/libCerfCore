#include "../vr41xx/vr41xx_scan_rep_kiu.h"

#include <cerfcore/boards/board_context.h>
#include "vr4102_id.h"
#include <cerfcore/core/cerf_emulator.h>

#include <cstdint>

namespace cerfcore {

namespace {

constexpr Vr41xxKiuModel kModel = {
    .stprep_zero_count        = 64u,
    .scanstart_auto_clear     = false,
    .keyen_scanline_interlock = false,
    .keyen_stop_deferred      = false,
    .scanstart_while_running_defined = false,
    .scanstp_completes_when_idle     = false,
    .gpen_retained_on_other_reset = false,
    .gpen_survives_kiurst         = false,
    .scanline_none_drives_all     = false,
    .scan_pins_drive_piod         = false,
    .count_bias                   = 1u,
    .zero_count_rfu               = true,
};

class Vr4102Kiu : public Vr41xxScanRepKiu {
public:
    using Vr41xxScanRepKiu::Vr41xxScanRepKiu;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4102;
    }

protected:
    const Vr41xxKiuModel& Model() const override { return kModel; }
};

}

REGISTER_SERVICE_AS(Vr4102Kiu, Vr41xxKiu);

}
