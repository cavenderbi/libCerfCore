#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../lcd/display_size_latch.h"

#include <atomic>
#include <cstdint>

namespace cerfcore {

class CasioCassiopeiaA10Lcd : public Peripheral {
public:
    static constexpr uint32_t kWidth       = 480u;
    static constexpr uint32_t kHeight      = 240u;
    static constexpr uint32_t kPlaneStride = 64u;
    static constexpr uint32_t kMaxPlanes   = 3u;
    static constexpr uint32_t kShownPlanes = 2u;

    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

    uint8_t  ReadByte(uint32_t addr) override;
    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

    bool     PanelOn() const;
    uint32_t Planes() const;
    uint32_t FramePa() const;

private:
    static constexpr uint32_t kSetupRegisters = 9u;

    uint8_t PlaneRead(uint32_t addr);
    bool    Convert(uint16_t pixels, uint32_t planes);
    void    Reset();

    std::atomic<uint16_t> control_{0u};
    uint16_t              setup_[kSetupRegisters]{};
    uint8_t               outputs_[kMaxPlanes]{};
    uint8_t               converted_planes_ = 0u;
    DisplaySizeLatch      size_latch_;
};

}
