#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/vr41xx/vr41xx_kiu_keyboard_input.h"
#include <cerfcore/boards/board_context.h>
#include "casio_pocketpostpet_family_id.h"
#include "casio_pocketpostpet_asic.h"

#include <cstdint>

namespace cerfcore {

namespace {

class CasioPocketpostpetKeyboardInput : public Vr41xxKiuKeyboardInput {
public:
    using Vr41xxKiuKeyboardInput::Vr41xxKiuKeyboardInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioPocketpostpet;
    }

protected:
    void OnBoardKey(uint32_t code, bool pressed) override {
        emu_.Get<CasioPocketpostpetAsic>().SetSideButton(static_cast<uint16_t>(code & 0xFFu), pressed);
    }
};

}

REGISTER_SERVICE(CasioPocketpostpetKeyboardInput);

}
