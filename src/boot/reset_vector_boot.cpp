#include "boot_mode.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

namespace {

class ResetVectorBoot : public BootMode {
public:
    using BootMode::BootMode;

    bool ShouldRegister() override {
        const RomPlacingMode mode = emu_.Get<BoardContext>().GetRomPlacingMode();
        return mode == RomPlacingMode::ResetVector || mode == RomPlacingMode::Unknown;
    }

    uint32_t ColdEntryPa() override { return 0; }
    uint32_t ColdStackPa() override { return 0; }
};

}

REGISTER_SERVICE_AS(ResetVectorBoot, BootMode);

}
