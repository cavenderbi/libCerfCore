#include "hp_jornada_540_lcd_clock.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_cpg.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

bool HpJornada540LcdClock::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

void HpJornada540LcdClock::OnReady() {
    ckio_hz_ = emu_.Get<Sh77xxCpg>().AttachToCkioPin();
}

void HpJornada540LcdClock::SetDivider(uint32_t divider) {
    if (divider == divider_) return;
    divider_ = divider;
    listeners_.Call();
}

REGISTER_SERVICE(HpJornada540LcdClock);

}
