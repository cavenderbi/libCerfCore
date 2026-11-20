#include <cerfcore/host/keyboard_input.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/keyboard_map.h>
#include <cerfcore/host/keyboard_router.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_key_matrix.h"

#include <cstdint>

namespace cerfcore {

namespace {

class CasioCassiopeiaA10KeyboardInput : public KeyboardInput {
public:
    using KeyboardInput::KeyboardInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        map_    = &emu_.Get<KeyboardMap>();
        matrix_ = &emu_.Get<CasioCassiopeiaA10KeyMatrix>();
        emu_.Get<KeyboardRouter>().Register(this);
    }

    void OnHostKey(uint8_t vk, bool key_up) override {
        uint32_t cell = 0u;
        if (!map_->BaseDeviceCode(vk, cell)) return;
        matrix_->SetKey(cell, !key_up);
    }

private:
    KeyboardMap*                 map_    = nullptr;
    CasioCassiopeiaA10KeyMatrix* matrix_ = nullptr;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10KeyboardInput);

}
