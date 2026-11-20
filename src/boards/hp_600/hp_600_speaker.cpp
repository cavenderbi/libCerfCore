#include "../sh77xx_dac_speaker.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_600_audio_switches.h"
#include "hp_600_family_id.h"

#include <cstdint>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kSpeakerChannel = 1u;

class Hp600Speaker : public Sh77xxDacSpeaker {
public:
    using Sh77xxDacSpeaker::Sh77xxDacSpeaker;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        switches_ = &emu_.Get<Hp600AudioSwitches>();
        Sh77xxDacSpeaker::OnReady();
    }

protected:
    uint32_t DacChannel() const override { return kSpeakerChannel; }

    bool SpeakerOn() const override { return switches_->SpeakerOn(); }

    void AddSpeakerSwitchListener(std::function<void()> listener) override {
        switches_->AddListener(std::move(listener));
    }

private:
    Hp600AudioSwitches* switches_ = nullptr;
};

}

REGISTER_SERVICE(Hp600Speaker);

}
