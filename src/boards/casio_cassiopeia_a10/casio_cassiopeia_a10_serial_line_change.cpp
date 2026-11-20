#include <cerfcore/core/service.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_chip_intc.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_serial_connector.h"
#include "casio_cassiopeia_a10_serial_event_enable.h"
#include "casio_cassiopeia_a10_serial_event_flags.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

namespace {

class CasioCassiopeiaA10SerialLineChange : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        enable_    = &emu_.Get<CasioCassiopeiaA10SerialEventEnable>();
        flags_     = &emu_.Get<CasioCassiopeiaA10SerialEventFlags>();
        chip_intc_ = &emu_.Get<CasioCassiopeiaA10ChipIntc>();
        enable_->AddChangeListener([this] { DriveRequest(); });
        flags_->AddChangeListener([this] { DriveRequest(); });
        emu_.Get<CasioCassiopeiaA10SerialConnector>().AddCarrierListener(
            [this](bool carrier) { OnCarrierChange(carrier); });
        DriveRequest();
    }

private:
    void OnCarrierChange(bool carrier) {
        const uint16_t enable = enable_->Value();
        if ((enable & kCasioCassiopeiaA10LineChangeEnable) == 0u) return;
        const bool falling_selected = (enable & kCasioCassiopeiaA10LineFallingEdge) != 0u;
        if (falling_selected == carrier) return;
        flags_->Latch(kCasioCassiopeiaA10LineChangeFlag);
    }

    void DriveRequest() {
        std::lock_guard<std::mutex> lk(mtx_);
        const bool request = (enable_->Value() & kCasioCassiopeiaA10LineChangeEnable) != 0u &&
                             (flags_->Value() & kCasioCassiopeiaA10LineChangeFlag) != 0u;
        chip_intc_->SetInputLevel(CasioCassiopeiaA10ChipInput::kSerialLineChange, request);
    }

    std::mutex                           mtx_;
    CasioCassiopeiaA10SerialEventEnable* enable_    = nullptr;
    CasioCassiopeiaA10SerialEventFlags*  flags_     = nullptr;
    CasioCassiopeiaA10ChipIntc*          chip_intc_ = nullptr;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10SerialLineChange);

}
