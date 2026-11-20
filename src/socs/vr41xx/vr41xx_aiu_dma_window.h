#pragma once

#include "vr41xx_aiu_dma.h"
#include "../reg_window_impl.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

template <const std::string_view& Soc, const cerf_reg_window_detail::RegWindowModel& M, class Iface>
class Vr41xxAiuDmaWindow : public cerf_reg_window_detail::RegWindowBase<Soc, M, Iface> {
protected:
    using Window = cerf_reg_window_detail::RegWindowBase<Soc, M, Iface>;

public:
    using Window::Window;

    uint16_t ReadHalf(uint32_t addr) final {
        auto& aiu = this->emu_.template Get<Vr41xxAiuDma>();
        aiu.SyncDma();
        return ReadSynced(addr, aiu);
    }

    void WriteHalf(uint32_t addr, uint16_t value) final {
        auto& aiu = this->emu_.template Get<Vr41xxAiuDma>();
        aiu.WriteDmaRegister([&] { WriteSynced(addr, value, aiu); });
    }

protected:
    virtual uint16_t ReadSynced(uint32_t addr, Vr41xxAiuDma& aiu) {
        (void)aiu;
        return Window::ReadHalf(addr);
    }

    virtual void WriteSynced(uint32_t addr, uint16_t value, Vr41xxAiuDma& aiu) {
        (void)aiu;
        Window::WriteHalf(addr, value);
    }
};

}
