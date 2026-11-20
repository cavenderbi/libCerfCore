#include "../../socs/sh77xx/sh77xx_scif_external_clock.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "casio_cassiopeia_a10_baud_divisor.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_serial_control.h"

#include <cstdint>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint16_t kRateSelect          = 0x00C0u;
constexpr uint16_t kRateDivided         = 0x0000u;
constexpr uint16_t kRate38400           = 0x0040u;
constexpr uint16_t kRate115200          = 0x0080u;
constexpr uint32_t kClocksPerBit        = 16u;
constexpr uint32_t kDividedBaseBitRate  = 57600u;
constexpr uint32_t kSelected38400       = 38400u;
constexpr uint32_t kSelected115200      = 115200u;

class CasioCassiopeiaA10ScifExternalClock : public Sh77xxScifExternalClock {
public:
    using Sh77xxScifExternalClock::Sh77xxScifExternalClock;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        control_ = &emu_.Get<CasioCassiopeiaA10SerialControl>();
        divisor_ = &emu_.Get<CasioCassiopeiaA10BaudDivisor>();
    }

    uint32_t ClockHz(const Sh77xxScifChannel&) const override {
        const uint16_t select = static_cast<uint16_t>(control_->Value() & kRateSelect);
        switch (select) {
        case kRateDivided:
            return kClocksPerBit * kDividedBaseBitRate / (divisor_->Value() + 1u);
        case kRate38400:
            return kClocksPerBit * kSelected38400;
        case kRate115200:
            return kClocksPerBit * kSelected115200;
        default:
            emu_.Get<Fatal>().Die("CasioCassiopeiaA10ScifExternalClock: H'230 rate select 0x%04X", select);
        }
    }

    void AddChangeListener(CallbackList<>::Callback listener) override {
        control_->AddChangeListener(listener);
        divisor_->AddChangeListener(std::move(listener));
    }

private:
    CasioCassiopeiaA10SerialControl* control_ = nullptr;
    CasioCassiopeiaA10BaudDivisor*   divisor_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10ScifExternalClock, Sh77xxScifExternalClock);

}
