#include "../../peripherals/hitachi_hd64461/hd64461_timer_output.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../sh77xx_microphone_trigger.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kTimer0 = 0u;

class Hp600Hd64461TimerOutput : public Hd64461TimerOutput {
public:
    using Hd64461TimerOutput::Hd64461TimerOutput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override { trigger_ = &emu_.Get<Sh77xxMicrophoneTrigger>(); }

    void Pulse(uint32_t channel, uint32_t rate_hz) override {
        if (channel != kTimer0) {
            emu_.Get<Fatal>().Die("Hp600Hd64461TimerOutput: TMO%u# pulse at %u Hz", channel, rate_hz);
        }
        trigger_->Pulse(rate_hz);
    }

private:
    Sh77xxMicrophoneTrigger* trigger_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp600Hd64461TimerOutput, Hd64461TimerOutput);

}
