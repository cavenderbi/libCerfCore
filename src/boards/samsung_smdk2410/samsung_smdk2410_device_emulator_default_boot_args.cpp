#include "samsung_smdk2410_device_emulator_boot_args.h"

#include <cerfcore/core/cerf_emulator.h>
#include "samsung_smdk2410_id.h"

namespace cerfcore {

namespace {

constexpr SamsungSmdk2410DeviceEmulatorArgsBlock kBlocks[] = {
    kSamsungSmdk2410DeviceEmulatorVideoArgs,
};

class SamsungSmdk2410DeviceEmulatorDefaultBootArgs : public SamsungSmdk2410DeviceEmulatorBootArgs {
public:
    using SamsungSmdk2410DeviceEmulatorBootArgs::SamsungSmdk2410DeviceEmulatorBootArgs;

protected:
    bool MatchesBoard(std::string_view board_id) const override {
        return board_id != BoardId::SamsungSmdk2410;
    }

    std::span<const SamsungSmdk2410DeviceEmulatorArgsBlock> Blocks() const override { return kBlocks; }
};

}

REGISTER_SERVICE_AS_FALLBACK(SamsungSmdk2410DeviceEmulatorDefaultBootArgs,
                             SamsungSmdk2410DeviceEmulatorBootArgs);

}
