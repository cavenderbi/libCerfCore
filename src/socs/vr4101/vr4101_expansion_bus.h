#pragma once

#include "../../peripherals/pcic82365/isa_host_bus_rule.h"

#include <cstdint>

namespace cerfcore {

enum class Vr4101DeviceMode : uint8_t { kEightBit, kSixteenBit };

inline constexpr uint32_t kVr4101ExpansionAreaBytes = 0x00200000u;

constexpr uint32_t Vr4101ExpansionIoBase(Vr4101DeviceMode mode) {
    return mode == Vr4101DeviceMode::kEightBit ? 0x16000000u : 0x17000000u;
}

constexpr uint32_t Vr4101ExpansionMemBase(Vr4101DeviceMode mode) {
    return mode == Vr4101DeviceMode::kEightBit ? 0x14000000u : 0x15000000u;
}

constexpr bool Vr4101ExpansionAccessAllowed(Vr4101DeviceMode mode, bool sixteen_bit_device, unsigned bytes,
                                            uint32_t addr) {
    const bool odd = (addr & 1u) != 0u;
    if (sixteen_bit_device) return bytes != 1u || !odd || mode == Vr4101DeviceMode::kSixteenBit;
    return bytes == 1u && (!odd || mode == Vr4101DeviceMode::kEightBit);
}

template <Vr4101DeviceMode kMode>
class Vr4101ExpansionHost final : public IsaHostBusRule {
public:
    static constexpr bool kRestricted = true;

    bool AllowsCycle(bool sixteen_bit_device, unsigned bytes, uint32_t off) const override {
        return Vr4101ExpansionAccessAllowed(kMode, sixteen_bit_device, bytes, off);
    }
};

}
