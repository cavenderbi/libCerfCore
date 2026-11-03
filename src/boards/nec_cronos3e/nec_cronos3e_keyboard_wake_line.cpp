#include <cerfcore/core/service.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/vr41xx/vr41xx_giu.h"
#include "../../socs/vr41xx/vr41xx_kiu.h"
#include "nec_mobilepro_780_id.h"
#include "nec_mobilepro_790_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr int     kWakeGpioPin     = 0;
constexpr uint8_t kWiredKportLines = 0xF0u;

class NecCronos3eKeyboardWakeLine : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override {
        const auto board = emu_.Get<BoardContext>().GetBoardId();
        return board == BoardId::NecMobilepro790 || board == BoardId::NecMobilepro780;
    }

    void OnReady() override {
        auto& giu = emu_.Get<Vr41xxGiu>();
        emu_.Get<Vr41xxKiu>().RegisterKportListener([&giu](uint8_t kport) {
            giu.SetPinLevel(kWakeGpioPin, (kport & kWiredKportLines) != 0u);
        });
    }
};

}

REGISTER_SERVICE(NecCronos3eKeyboardWakeLine);

}
