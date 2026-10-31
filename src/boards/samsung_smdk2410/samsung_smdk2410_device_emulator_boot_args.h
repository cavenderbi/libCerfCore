#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace cerfcore {

struct SamsungSmdk2410DeviceEmulatorArgsBlock {
    uint32_t signature_pa;
    uint32_t signature;
    uint32_t width_pa;
    uint32_t height_pa;
    uint32_t bits_per_pixel_pa;
    bool     has_soft_reset_flag;
    uint32_t soft_reset_pa;
};

inline constexpr SamsungSmdk2410DeviceEmulatorArgsBlock kSamsungSmdk2410DeviceEmulatorVideoArgs = {
    .signature_pa        = 0x30020044u,
    .signature           = 0xDE12DE34u,
    .width_pa            = 0x30020048u,
    .height_pa           = 0x3002004Au,
    .bits_per_pixel_pa   = 0x3002004Cu,
    .has_soft_reset_flag = false,
    .soft_reset_pa       = 0u,
};

class SamsungSmdk2410DeviceEmulatorBootArgs : public Service {
public:
    using Service::Service;

    bool ShouldRegister() final;
    void OnReady() final;

protected:
    virtual bool MatchesBoard(std::string_view board_id) const = 0;
    virtual std::span<const SamsungSmdk2410DeviceEmulatorArgsBlock> Blocks() const = 0;

private:
    void WriteBlocks();
};

}
