#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_input.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/host/keyboard_router.h>
#include <cerfcore/boards/board_context.h>
#include "casio_jx670_companion.h"
#include "casio_jx670_family_id.h"

#include <cstdint>
#include <string>

namespace cerfcore {

namespace {

class CasioJx670Keyboard : public KeyboardInput {
public:
    using KeyboardInput::KeyboardInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioJx670;
    }

    void OnReady() override {
        emu_.Get<KeyboardRouter>().Register(this);
    }

    static constexpr char kSourceName[] = "Cassiopeia buttons";
    std::string SourceName() const override { return kSourceName; }

    void OnHostKey(uint8_t vk, bool key_up) override {
        uint32_t bit = 0;
        if (!emu_.Get<KeyboardMap>().BaseDeviceCode(vk, bit)) return;
        emu_.Get<CasioJx670Companion>().SetButtons(static_cast<uint16_t>(1u << bit), !key_up);
    }
};

}

REGISTER_SERVICE(CasioJx670Keyboard);

}
