#include "../../host/boot_sleep.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10BootSleep : public BootSleep {
public:
    using BootSleep::BootSleep;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    ResumeSource Waker() const override { return ResumeSource::User; }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10BootSleep, BootSleep);

}
