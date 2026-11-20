#include "../../peripherals/intel_82365sl/intel_82365sl_board.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/vr41xx/vr41xx_icu.h"
#include "nec_mobilepro_200_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint8_t  kWiredIrq        = 3u;
constexpr uint16_t kSysint1PcmciaIrq = 1u << 4;

class NecMobilepro200Intel82365slBoard : public Intel82365slBoard {
public:
    using Intel82365slBoard::Intel82365slBoard;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro200;
    }

    uint8_t WiredIrqSelect() const override { return kWiredIrq; }
    bool    CardInputPullupsSwitched() const override { return true; }
    bool    HostSamplesMemcs16() const override { return false; }
    bool    ResetDriveAtReset(ResetLineKind kind) override {
        return kind == ResetLineKind::Rtc || emu_.Get<GuestCpuReset>().DeliveredResetWasResume();
    }
    void    SetIrqLevel(bool level) override { emu_.Get<Vr41xxIcu>().SetSysint1Source(kSysint1PcmciaIrq, level); }
};

}

REGISTER_SERVICE_AS(NecMobilepro200Intel82365slBoard, Intel82365slBoard);

}
