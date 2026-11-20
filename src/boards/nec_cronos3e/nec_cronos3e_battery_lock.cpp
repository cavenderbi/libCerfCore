#include "../nec_vr41xx_battery_lock.h"

#include <cerfcore/core/cerf_emulator.h>
#include "nec_cronos3e_family_id.h"

namespace cerfcore {

namespace {

constexpr int kBatteryLockPin = 12;

class NecCronos3eBatteryLock : public NecVr41xxBatteryLock {
public:
    using NecVr41xxBatteryLock::NecVr41xxBatteryLock;

protected:
    std::string_view BatteryLockFamilyId() const override { return BoardFamilyId::NecCronos3e; }
    int              BatteryLockPin() const override { return kBatteryLockPin; }
};

}

REGISTER_SERVICE(NecCronos3eBatteryLock);

}
