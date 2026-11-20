#include "../cirrus_cd1284/cd1284.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_debug_board_placement.h"

namespace cerfcore {

namespace {

constexpr uint8_t kGpioInputsLow     = 0x00u;
constexpr uint8_t kHostHandshakeIdle = 0x0Fu;

class HpDebugBoardCd1284 : public Cd1284 {
public:
    using Cd1284::Cd1284;

    bool ShouldRegister() override {
        return HpDebugBoardPlacement::FittedOnFamily(emu_.Get<BoardContext>().GetBoardFamilyId());
    }

    void OnReady() override {
        const HpDebugBoardPlacement& placement = emu_.Get<HpDebugBoardPlacement>();
        base_pa_        = placement.BasePa();
        register_shift_ = placement.Cd1284RegisterShift();
        Cd1284::OnReady();
    }

    uint32_t MmioBase() const override { return base_pa_; }

protected:
    uint32_t RegisterShift() const override { return register_shift_; }
    uint8_t  GpioInputLevels() const override { return kGpioInputsLow; }
    uint8_t  HostHandshakeLevels() const override { return kHostHandshakeIdle; }

private:
    uint32_t base_pa_        = 0u;
    uint32_t register_shift_ = 0u;
};

}

REGISTER_SERVICE(HpDebugBoardCd1284);

}
