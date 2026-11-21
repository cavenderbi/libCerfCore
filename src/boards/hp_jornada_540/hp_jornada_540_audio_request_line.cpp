#include "hp_jornada_540_audio_request_line.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

bool HpJornada540AudioRequestLine::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

bool HpJornada540AudioRequestLine::Drive(bool asserted) {
    const bool changed = asserted != asserted_;
    asserted_          = asserted;
    return changed;
}

REGISTER_SERVICE(HpJornada540AudioRequestLine);

}
