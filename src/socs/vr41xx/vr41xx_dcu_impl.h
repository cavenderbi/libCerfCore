#pragma once

#include "vr41xx_aiu_dma_window.h"
#include "vr41xx_dcu.h"

#include <cerfcore/core/fatal.h>

#include <cstdint>
#include <string_view>

namespace cerfcore {

template <const std::string_view& Soc, const cerf_reg_window_detail::RegWindowModel& M>
class Vr41xxDcuImpl : public Vr41xxAiuDmaWindow<Soc, M, Vr41xxDcu> {
    using Synced = Vr41xxAiuDmaWindow<Soc, M, Vr41xxDcu>;

public:
    using Synced::Synced;

    bool DmaEnabled(uint16_t msk_bit) const override {
        return (this->StoredReg(kSenIndex) & 1u) != 0u && (this->StoredReg(kMskIndex) & msk_bit) != 0u;
    }

protected:
    uint16_t ReadSynced(uint32_t addr, Vr41xxAiuDma& aiu) override {
        if (addr - M.base == kOffReq) return aiu.DmaRequests();
        return Synced::ReadSynced(addr, aiu);
    }

    void WriteSynced(uint32_t addr, uint16_t value, Vr41xxAiuDma& aiu) override {
        const uint32_t off = addr - M.base;
        if (off == kOffRst && (value & 1u) == 0u) {
            this->emu_.template Get<Fatal>().Die("Vr41xxDcu: DMARSTREG write 0x%04X resets the DMA controller", value);
        }
        if (off == kOffMsk) {
            const uint16_t rising = static_cast<uint16_t>(value & ~this->StoredReg(kMskIndex));
            if (rising & aiu.DmaRequests()) {
                this->emu_.template Get<Fatal>().Die(
                    "Vr41xxDcu: DMAMSKREG write 0x%04X enables a unit with a DMA request pending (requests 0x%04X)",
                    value, aiu.DmaRequests());
            }
        }
        Synced::WriteSynced(addr, value, aiu);
    }

private:
    static constexpr uint32_t kOffRst   = 0x00u;
    static constexpr uint32_t kOffMsk   = 0x06u;
    static constexpr uint32_t kOffReq   = 0x08u;
    static constexpr uint32_t kSenIndex = 2u;
    static constexpr uint32_t kMskIndex = 3u;
};

}
