#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_input.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/host/keyboard_router.h>
#include "hp_600_family_id.h"
#include "hp_600_input_nets.h"
#include "../sh77xx_port_key_matrix.h"

#include <cstdint>

namespace cerfcore {

namespace {

class Hp600KeyboardInput : public KeyboardInput {
public:
    using KeyboardInput::KeyboardInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        map_    = &emu_.Get<KeyboardMap>();
        matrix_ = &emu_.Get<Sh77xxPortKeyMatrix>();
        nets_   = &emu_.Get<Hp600InputNets>();
        emu_.Get<KeyboardRouter>().Register(this);
    }

    void OnHostKey(uint8_t vk, bool key_up) override {
        uint32_t code = 0u;
        if (!map_->BaseDeviceCode(vk, code)) return;
        if (code >= kHp600InputNetCodeBase) {
            nets_->Drive(static_cast<Hp600InputNet>(code - kHp600InputNetCodeBase), key_up);
            return;
        }
        matrix_->SetKey(code, !key_up);
    }

private:
    KeyboardMap*         map_    = nullptr;
    Sh77xxPortKeyMatrix* matrix_ = nullptr;
    Hp600InputNets*      nets_   = nullptr;
};

}

REGISTER_SERVICE(Hp600KeyboardInput);

}
