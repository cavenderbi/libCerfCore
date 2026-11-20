#include "../vr41xx/vr41xx_aiu_dma_window.h"

#include <cerfcore/core/cerf_emulator.h>

#include <cstdint>
#include "vr4101_dma_addresses.h"
#include "vr4101_id.h"

namespace cerfcore {

namespace {

using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

constexpr ReadKind  kRd = ReadKind::kStored;
constexpr WriteKind kWr = WriteKind::kStored;

constexpr uint16_t kLowMask   = 0xFFFEu;
constexpr uint16_t kHighMask  = 0x001Fu;
constexpr uint16_t kHighReset = 0x001Fu;

constexpr RegWindowModel kModel = {
    0x0B000020u,
    0x20u,
    10u,
    true,
    {
        { kRd, kWr, kLowMask, 0xF800u },
        { kRd, kWr, kHighMask, kHighReset },
        { kRd, kWr, kLowMask, 0xF000u },
        { kRd, kWr, kHighMask, kHighReset },
        { kRd, kWr, kLowMask, 0xE800u },
        { kRd, kWr, kHighMask, kHighReset },
        { kRd, kWr, kLowMask, 0xE000u },
        { kRd, kWr, kHighMask, kHighReset },
        { kRd, kWr, kLowMask, 0xD800u },
        { kRd, kWr, kHighMask, kHighReset },
    },
};

class Vr4101Dmaau : public Vr41xxAiuDmaWindow<SocId::Vr4101, kModel, Vr4101DmaAddresses> {
public:
    using Vr41xxAiuDmaWindow::Vr41xxAiuDmaWindow;

    uint32_t Address(Vr4101DmaChannel channel) const override {
        const uint32_t low = LowIndex(channel);
        return StoredReg(low) | (static_cast<uint32_t>(StoredReg(low + 1u)) << 16);
    }

    bool Step(Vr4101DmaChannel channel) override {
        const uint32_t low    = LowIndex(channel);
        const uint16_t value  = StoredReg(low);
        const uint16_t offset = static_cast<uint16_t>((value + 2u) & kSpaceOffset);
        SetStoredReg(low, static_cast<uint16_t>((value & ~kSpaceOffset) | offset));
        return (offset & (kPageBytes - 1u)) == 0u;
    }

    uint32_t HalfwordsToBoundary(Vr4101DmaChannel channel) const override {
        const uint16_t in_page = static_cast<uint16_t>(StoredReg(LowIndex(channel)) & kSpaceOffset & (kPageBytes - 1u));
        return (kPageBytes - in_page) / 2u;
    }

private:
    static constexpr uint16_t kSpaceOffset = 0x07FEu;
    static constexpr uint16_t kPageBytes   = 0x0400u;

    static uint32_t LowIndex(Vr4101DmaChannel channel) { return static_cast<uint32_t>(channel) * 2u; }
};

}

REGISTER_SERVICE_AS(Vr4101Dmaau, Vr4101DmaAddresses);

}
