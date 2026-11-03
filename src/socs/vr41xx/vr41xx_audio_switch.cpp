#include "vr41xx_audio_switch.h"

#include <cerfcore/core/cerf_emulator.h>

#include <utility>

namespace cerfcore {

bool Vr41xxGpioAudioSwitch::SpeakerOn() { return PinAt(SpeakerGpio(), SpeakerOnLevel()); }

bool Vr41xxGpioAudioSwitch::PinAt(int gpio, Vr41xxPinDrive level) {
    return emu_.Get<Vr41xxGiu>().OutputDrive(gpio) == level;
}

void Vr41xxGpioAudioSwitch::RegisterChangeListener(std::function<void()> fn) {
    emu_.Get<Vr41xxGiu>().RegisterOutputDriveListener(std::move(fn));
}

}
