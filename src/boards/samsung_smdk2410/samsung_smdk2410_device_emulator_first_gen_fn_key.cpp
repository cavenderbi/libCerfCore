#include "samsung_smdk2410_fn_key.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "samsung_smdk2410_device_emulator_first_gen_boards.h"

namespace cerfcore {

namespace {

class SamsungSmdk2410DeviceEmulatorFirstGenFnKey : public SamsungSmdk2410FnKey {
public:
    using SamsungSmdk2410FnKey::SamsungSmdk2410FnKey;

    bool ShouldRegister() override {
        return IsDeviceEmulatorFirstGenBoard(emu_.Get<BoardContext>().GetBoardId());
    }

    uint8_t Scancode() const override { return 0x63; }
    bool    HasApp1Key() const override { return true; }
};

}

REGISTER_SERVICE_AS(SamsungSmdk2410DeviceEmulatorFirstGenFnKey, SamsungSmdk2410FnKey);

}
