#include "../../peripherals/philips_ucb1200/ucb1x00_codec.h"

#include <cerfcore/boards/board_context.h>
#include "philips_nino_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/pr31x00/pr31x00_io.h"

#include <cstdint>

namespace cerfcore {

namespace {

/* philips_nino_300 sib.dll sub_18D1824 pulses MFIODOUT bit 19 high, low, high before
   sub_18D1768 reads the codec ID. */
constexpr uint32_t kMfioCodecReset = 1u << 19;

class PhilipsNinoCodecResetPin : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::PhilipsNino;
    }

    void OnReady() override {
        emu_.Get<Pr31x00Io>().RegisterMfioOutObserver([this](const Pr31x00Pins& pins) {
            using Pin = Ucb1x00Codec::ResetPin;
            const Pin level = (pins.Low() & kMfioCodecReset) != 0u    ? Pin::Low
                              : (pins.driven & kMfioCodecReset) != 0u ? Pin::High
                                                                      : Pin::Floating;
            emu_.Get<Ucb1x00Codec>().DriveResetPin(level);
        });
    }
};

}

REGISTER_SERVICE(PhilipsNinoCodecResetPin);

}
