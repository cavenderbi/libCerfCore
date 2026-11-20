#include "casio_cassiopeia_a10_nmi_line.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_register_00e.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10NmiLine);

bool CasioCassiopeiaA10NmiLine::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
}

void CasioCassiopeiaA10NmiLine::OnReady() {
    intc_         = &emu_.Get<Sh77xxIntc>();
    register_00e_ = &emu_.Get<CasioCassiopeiaA10Register00E>();
    register_00e_->AddChangeListener([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        DriveLocked();
    });
    std::lock_guard<std::mutex> lk(mtx_);
    DriveLocked();
}

void CasioCassiopeiaA10NmiLine::SetWakeRequest(bool asserted) {
    std::lock_guard<std::mutex> lk(mtx_);
    wake_request_ = asserted;
    DriveLocked();
}

void CasioCassiopeiaA10NmiLine::SetOnKey(bool down) {
    std::lock_guard<std::mutex> lk(mtx_);
    on_key_down_ = down;
    DriveLocked();
}

bool CasioCassiopeiaA10NmiLine::OnKeyDown() {
    std::lock_guard<std::mutex> lk(mtx_);
    return on_key_down_;
}

void CasioCassiopeiaA10NmiLine::DriveLocked() {
    const bool latch_drives = (register_00e_->Value() & kCasioCassiopeiaA10WakeLatchNmi) != 0u;
    intc_->SetNmiLevel(!(wake_request_ && latch_drives) && !on_key_down_);
}

}
