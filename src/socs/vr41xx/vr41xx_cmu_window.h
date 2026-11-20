#pragma once

#include "vr41xx_cmu.h"
#include "../reg_window_impl.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

template <const std::string_view& Soc, const cerf_reg_window_detail::RegWindowModel& M>
class Vr41xxCmuWindow : public cerf_reg_window_detail::RegWindowBase<Soc, M, Vr41xxCmu> {
    using Base = cerf_reg_window_detail::RegWindowBase<Soc, M, Vr41xxCmu>;

public:
    using Base::Base;

protected:
    uint16_t ClockMask() const override { return this->StoredReg(0); }
    void     BeforeStore(uint32_t, uint16_t value) override { this->ClockMaskWriting(value); }
};

}
