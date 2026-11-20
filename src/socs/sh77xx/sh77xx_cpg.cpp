#include "sh77xx_cpg.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../cpu/sh/sh_board_clock.h"

namespace cerfcore {

uint64_t Sh77xxCpg::AttachToCkioPin() {
    ckio_pin_attached_ = true;
    return BoardCkioHz();
}

GuestCycleClock::Rate Sh77xxCpg::InternalCkioRate() const { return GuestCycleClock::Rate{BoardCkioHz(), 1u}; }

uint64_t Sh77xxCpg::BoardCkioHz() const {
    return emu_.Get<ShBoardClock>().CkioHz();
}

}
