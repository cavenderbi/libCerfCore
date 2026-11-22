#include "hp_jornada_540_key_wake.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/hibernation.h>
#include <cerfcore/state/state_stream.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

REGISTER_SERVICE(HpJornada540KeyWake);

bool HpJornada540KeyWake::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

void HpJornada540KeyWake::OnReady() {
    intc_ = &emu_.Get<Sh77xxIntc>();
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { Publish(); });
    Publish();
}

void HpJornada540KeyWake::Raise() {
    pending_.store(true, std::memory_order_release);
    Publish();
}

void HpJornada540KeyWake::Clear() {
    pending_.store(false, std::memory_order_release);
    Publish();
}

bool HpJornada540KeyWake::Pending() const { return pending_.load(std::memory_order_acquire); }

void HpJornada540KeyWake::SaveState(StateWriter& w) const { w.Write<uint8_t>("key_wake", Pending() ? 1u : 0u); }

void HpJornada540KeyWake::RestoreState(StateReader& r) {
    uint8_t pending = 0u;
    r.Read("key_wake", pending);
    pending_.store(pending != 0u, std::memory_order_release);
}

void HpJornada540KeyWake::Publish() { intc_->SetPinLevel(Sh77xxIntcPin::kPint0, !Pending()); }

}
