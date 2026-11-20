#include "../vr41xx/vr41xx_pmu_impl.h"
#include "vr4101_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

using cerf_vr41xx_pmu_detail::Vr41xxPmu2Model;
using cerf_vr41xx_pmu_detail::Vr41xxPmuBase;
using cerf_vr41xx_pmu_detail::Vr41xxPmuModel;

constexpr Vr41xxPmuModel kModel = {
    .base                     = 0x0B0000A0u,
    .size                     = 0x20u,
    .int_w1c                  = 0x033Fu,
    .int_sw_rw                = 0x00C0u,
    .int_power_on             = 0x0010u,
    .cnt_writable             = 0x0084u,
    .cnt_fixed_read           = 0x0002u,
    .cnt_power_on             = 0x0002u,
    .wait_wmask               = 0u,
    .wait_power_on            = 0u,
    .int_warm_cause           = 0x0008u,
    .int_cold_cause           = 0x0010u,
    .int_power_wake           = 0x0001u,
    .cnt_write_zero           = 0xFF78u,
    .cnt_write_one            = 0u,
    .int_held_on_reset        = 0xFFFFu,
    .int_watchdog_cause       = 0x0004u,
    .power_holds_wake_factors = false,
    .cnt_held_on_reset        = 0xFFFFu,
    .second_pair              = false,
    .dcd_wake_level           = true,
};

constexpr Vr41xxPmu2Model kModel2 = {
    .int2_w1c           = 0u,
    .int2_write_zero    = 0u,
    .cnt2_writable      = 0u,
    .cnt2_write_zero    = 0u,
    .cnt2_held_on_reset = 0u,
    .cnt2_soft_reset    = 0u,
};

class Vr4101Pmu : public Vr41xxPmuBase<SocId::Vr4101, kModel, kModel2> {
public:
    using Vr41xxPmuBase::Vr41xxPmuBase;
};

}

REGISTER_SERVICE_AS(Vr4101Pmu, Vr41xxPmu);

}
