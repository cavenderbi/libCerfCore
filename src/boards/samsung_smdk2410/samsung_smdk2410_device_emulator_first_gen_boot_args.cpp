#include "samsung_smdk2410_device_emulator_boot_args.h"

#include <cerfcore/core/cerf_emulator.h>
#include "samsung_smdk2410_device_emulator_first_gen_boards.h"

namespace cerfcore {

namespace {

constexpr SamsungSmdk2410DeviceEmulatorArgsBlock kBootBlock = {
    .signature_pa        = 0x301FF004u,
    .signature           = 0x544F4F42u,
    .width_pa            = 0x301FF072u,
    .height_pa           = 0x301FF074u,
    .bits_per_pixel_pa   = 0x301FF078u,
    .has_soft_reset_flag = true,
    .soft_reset_pa       = 0x301FF080u,
};

constexpr SamsungSmdk2410DeviceEmulatorArgsBlock kBlocks[] = {
    kSamsungSmdk2410DeviceEmulatorVideoArgs,
    kBootBlock,
};

class SamsungSmdk2410DeviceEmulatorFirstGenBootArgs : public SamsungSmdk2410DeviceEmulatorBootArgs {
public:
    using SamsungSmdk2410DeviceEmulatorBootArgs::SamsungSmdk2410DeviceEmulatorBootArgs;

protected:
    bool MatchesBoard(std::string_view board_id) const override {
        return IsDeviceEmulatorFirstGenBoard(board_id);
    }

    std::span<const SamsungSmdk2410DeviceEmulatorArgsBlock> Blocks() const override { return kBlocks; }
};

}

REGISTER_SERVICE_AS(SamsungSmdk2410DeviceEmulatorFirstGenBootArgs, SamsungSmdk2410DeviceEmulatorBootArgs);

}
