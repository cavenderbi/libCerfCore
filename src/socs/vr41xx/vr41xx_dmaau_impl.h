#pragma once

#include "vr41xx_aiu_dma_window.h"
#include "vr41xx_dmaau.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

template <const std::string_view& Soc, const cerf_reg_window_detail::RegWindowModel& M>
class Vr41xxDmaauImpl : public Vr41xxAiuDmaWindow<Soc, M, Vr41xxDmaau> {
public:
    using Vr41xxAiuDmaWindow<Soc, M, Vr41xxDmaau>::Vr41xxAiuDmaWindow;

    uint32_t Base(Vr41xxAiuChannel channel) const override { return Pair(BaseIndex(channel)); }

    uint32_t Address(Vr41xxAiuChannel channel) const override { return Pair(BaseIndex(channel) + 2u); }

    void SetAddress(Vr41xxAiuChannel channel, uint32_t pa) override {
        const uint32_t lo = BaseIndex(channel) + 2u;
        this->ApplyStoredWrite(lo * 2u, static_cast<uint16_t>(pa & M.reg[lo].wmask));
        this->ApplyStoredWrite((lo + 1u) * 2u, static_cast<uint16_t>((pa >> 16) & M.reg[lo + 1u].wmask));
    }

private:
    static constexpr uint32_t BaseIndex(Vr41xxAiuChannel channel) {
        return channel == Vr41xxAiuChannel::In ? 0u : 4u;
    }

    uint32_t Pair(uint32_t lo) const {
        return this->StoredReg(lo) | (static_cast<uint32_t>(this->StoredReg(lo + 1u)) << 16);
    }
};

}
