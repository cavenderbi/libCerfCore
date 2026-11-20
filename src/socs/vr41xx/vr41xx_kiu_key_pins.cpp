#include "vr41xx_kiu_key_pins.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../execution/engine_bridge.h"
#include <cerfengine/host_request_channel.h>
#include "../vr4101/vr4101_id.h"
#include "../vr4102/vr4102_id.h"
#include "../vr4111/vr4111_id.h"
#include "../vr4121/vr4121_id.h"

namespace cerfcore {

bool Vr41xxKiuKeyPins::ShouldRegister() {
    const std::string_view soc = emu_.Get<BoardContext>().GetSocId();
    return soc == SocId::Vr4101 || soc == SocId::Vr4102 || soc == SocId::Vr4111 || soc == SocId::Vr4121;
}

void Vr41xxKiuKeyPins::OnReady() {
    host_requests_ = &emu_.Get<EngineBridge>().Engine().Get<cerfengine::HostRequestChannel>();
}

void Vr41xxKiuKeyPins::Set(uint8_t matrix_index, bool pressed) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!held_.Set(matrix_index, pressed)) return;
        dirty_ = true;
    }
    host_requests_->Request();
}

bool Vr41xxKiuKeyPins::Take(uint16_t (&pins)[kRegs]) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!dirty_) return false;
    dirty_ = false;
    for (uint32_t i = 0; i < kRegs; ++i) pins[i] = held_.Line(i);
    return true;
}

void Vr41xxKiuKeyPins::Clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    held_.ReleaseAll();
    dirty_ = false;
}

REGISTER_SERVICE(Vr41xxKiuKeyPins);

}
