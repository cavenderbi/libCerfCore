#include "../../host/boot_sleep.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "nec_cronos3e_family_id.h"

namespace cerfcore {

namespace {

class NecCronos3eBootSleep : public BootSleep {
public:
    using BootSleep::BootSleep;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecCronos3e;
    }

    ResumeSource Waker() const override { return ResumeSource::Hardware; }
};

}

REGISTER_SERVICE_AS(NecCronos3eBootSleep, BootSleep);

}
