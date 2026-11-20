#include "sharp_mobilon_hc_key_matrix.h"

#include <cerfcore/boards/board_context.h>
#include "sharp_mobilon_hc_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../socs/pr31x00/pr31x00_io.h"

namespace cerfcore {

namespace {
constexpr uint32_t kSpinMfioPin = 13;
}  /* namespace */

bool SharpMobilonHcKeyMatrix::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SharpMobilonHc;
}

void SharpMobilonHcKeyMatrix::OnReady() {
    emu_.Get<PeripheralDispatcher>().Register(this);
}

uint16_t SharpMobilonHcKeyMatrix::ReadHalf(uint32_t addr) {
    const uint32_t off = addr - kBase;
    return cells_.Pressed((off >> 1) & kAllColumns);
}

void SharpMobilonHcKeyMatrix::SetKey(uint8_t col, uint8_t row, bool down) {
    if (col >= kColumnCount || row >= Cells::kColumns) return;
    if (!cells_.Set(col * Cells::kColumns + row, down)) return;
    emu_.Get<Pr31x00Io>().DriveMfioInput(kSpinMfioPin, cells_.Pressed(kAllColumns) != 0u);
}

void SharpMobilonHcKeyMatrix::RestoreState(StateReader&) { cells_.ReleaseAll(); }

REGISTER_SERVICE(SharpMobilonHcKeyMatrix);

}
