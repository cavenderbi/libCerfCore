#include "../../host/boot_sleep.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_jx670_family_id.h"

namespace cerfcore {

namespace {

class CasioJx670BootSleep : public BootSleep {
public:
    using BootSleep::BootSleep;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioJx670;
    }

    ResumeSource Waker() const override { return ResumeSource::Hardware; }
};

}

REGISTER_SERVICE_AS(CasioJx670BootSleep, BootSleep);

}
