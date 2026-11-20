#include "../nec_vr41xx_battery_lock.h"

#include <cerfcore/core/cerf_emulator.h>
#include "nec_mobilepro_200_family_id.h"

namespace cerfcore {

namespace {

class NecMobilepro200BatteryLock : public NecVr41xxBatteryLock {
public:
    using NecVr41xxBatteryLock::NecVr41xxBatteryLock;

protected:
    std::string_view BatteryLockFamilyId() const override { return BoardFamilyId::NecMobilepro200; }
    int              BatteryLockPin() const override { return 9; }
};

}

REGISTER_SERVICE(NecMobilepro200BatteryLock);

}
