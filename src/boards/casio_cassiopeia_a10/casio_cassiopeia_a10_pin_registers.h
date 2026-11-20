#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>
#include <mutex>

namespace cerfcore {

class CasioCassiopeiaA10ChipIntc;

inline constexpr uint32_t kCasioCassiopeiaA10Pin04C = 0x1000004Cu;
inline constexpr uint32_t kCasioCassiopeiaA10Pin04E = 0x1000004Eu;
inline constexpr uint32_t kCasioCassiopeiaA10Pin054 = 0x10000054u;
inline constexpr uint32_t kCasioCassiopeiaA10Pin056 = 0x10000056u;
inline constexpr uint32_t kCasioCassiopeiaA10Pin058 = 0x10000058u;
inline constexpr uint32_t kCasioCassiopeiaA10Pin05E = 0x1000005Eu;
inline constexpr uint32_t kCasioCassiopeiaA10Pin064 = 0x10000064u;
inline constexpr uint32_t kCasioCassiopeiaA10Pin066 = 0x10000066u;

class CasioCassiopeiaA10PinRegisters : public Peripheral {
public:
    static constexpr uint32_t kFirstPin  = 0x10000040u;
    static constexpr uint32_t kLastPin   = 0x10000066u;
    static constexpr uint32_t kRegisters = (kLastPin - kFirstPin) / 2u + 1u;

    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

    void SetLine(uint32_t addr, bool level);

private:
    bool LevelRequestLocked(uint32_t index) const;
    void DriveSourcesLocked();
    void Reset();

    std::mutex                  mtx_;
    CasioCassiopeiaA10ChipIntc* chip_intc_ = nullptr;
    uint16_t                    config_[kRegisters]{};
    uint16_t                    flags_[kRegisters]{};
    uint32_t                    lines_ = 0u;
};

}
