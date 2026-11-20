#include "nec_vr41xx_battery_lock.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../socs/vr41xx/vr41xx_giu.h"

namespace cerfcore {

bool NecVr41xxBatteryLock::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BatteryLockFamilyId();
}

void NecVr41xxBatteryLock::OnReady() {
    emu_.Get<Vr41xxGiu>().SetPinLevel(BatteryLockPin(), true);
}

}
