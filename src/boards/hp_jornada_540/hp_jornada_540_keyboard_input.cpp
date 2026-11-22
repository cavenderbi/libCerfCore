#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_input.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/host/keyboard_router.h>
#include "hp_jornada_540_buttons.h"
#include "hp_jornada_540_family_id.h"

#include <cstdint>
#include <string>

namespace cerfcore {

namespace {

class HpJornada540KeyboardInput : public KeyboardInput {
public:
    using KeyboardInput::KeyboardInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

    void OnReady() override {
        map_     = &emu_.Get<KeyboardMap>();
        buttons_ = &emu_.Get<HpJornada540Buttons>();
        emu_.Get<KeyboardRouter>().Register(this);
    }

    static constexpr char kSourceName[] = "Jornada buttons";
    std::string SourceName() const override { return kSourceName; }

    void OnHostKey(uint8_t vk, bool key_up) override {
        uint32_t code = 0u;
        if (!map_->BaseDeviceCode(vk, code)) return;
        buttons_->SetPressed(static_cast<HpJornada540Button>(code), !key_up);
    }

private:
    KeyboardMap*         map_     = nullptr;
    HpJornada540Buttons* buttons_ = nullptr;
};

}

REGISTER_SERVICE(HpJornada540KeyboardInput);

}
