#include "nec_vr41xx_isa_rf5c396_board.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../socs/vr41xx/vr41xx_giu.h"

namespace cerfcore {

namespace {

constexpr uint32_t kIsaIoBase     = 0x14000000u;
constexpr uint32_t kIsaMemBase    = 0x10000000u;
constexpr uint8_t  kWiredIrq      = 3u;
constexpr int      kGpiVs1Bank    = 1;
constexpr int      kIrqGiuPin     = 9;

}

uint32_t NecVr41xxIsaRf5c396Board::IsaIoBase() const {
    return kIsaIoBase;
}

uint32_t NecVr41xxIsaRf5c396Board::IsaMemBase() const {
    return kIsaMemBase;
}

uint8_t NecVr41xxIsaRf5c396Board::WiredIrqSelect() const {
    return kWiredIrq;
}

bool NecVr41xxIsaRf5c396Board::GpiWiredToVs1(int bank) const {
    return bank == kGpiVs1Bank;
}

void NecVr41xxIsaRf5c396Board::SetIrqLevel(bool level) {
    emu_.Get<Vr41xxGiu>().SetPinLevel(kIrqGiuPin, level);
}

}
