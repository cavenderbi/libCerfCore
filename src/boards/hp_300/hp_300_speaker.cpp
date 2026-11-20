#include "../sh77xx_dac_speaker.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "hp_300_family_id.h"
#include "hp_300_speaker_pin.h"

#include <cstdint>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kSpeakerChannel = 1u;

class Hp300Speaker : public Sh77xxDacSpeaker {
public:
    using Sh77xxDacSpeaker::Sh77xxDacSpeaker;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override {
        wiring_ = &emu_.Get<Sh77xxPortWiring>();
        ports_  = &emu_.Get<Sh77xxPortData>();
        Sh77xxDacSpeaker::OnReady();
    }

protected:
    uint32_t DacChannel() const override { return kSpeakerChannel; }

    bool SpeakerOn() const override { return (wiring_->NetLevels(kSh77xxPortSc) & kHp300PortScSpeakerOff) == 0u; }

    void AddSpeakerSwitchListener(std::function<void()> listener) override {
        ports_->AddDriveListener(std::move(listener));
    }

private:
    const Sh77xxPortWiring* wiring_ = nullptr;
    Sh77xxPortData*         ports_  = nullptr;
};

}

REGISTER_SERVICE(Hp300Speaker);

}
