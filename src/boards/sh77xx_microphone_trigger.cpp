#include "sh77xx_microphone_trigger.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../socs/sh77xx/sh77xx_adc.h"
#include "sh77xx_adc_microphone.h"

namespace cerfcore {

REGISTER_SERVICE(Sh77xxMicrophoneTrigger);

bool Sh77xxMicrophoneTrigger::ShouldRegister() { return emu_.TryGet<Sh77xxAdcMicrophone>() != nullptr; }

void Sh77xxMicrophoneTrigger::OnReady() {
    adc_        = &emu_.Get<Sh77xxAdc>();
    microphone_ = &emu_.Get<Sh77xxAdcMicrophone>();
}

void Sh77xxMicrophoneTrigger::Pulse(uint32_t rate_hz) {
    if (adc_->ExternalTrigger()) microphone_->SetRate(rate_hz);
}

}
