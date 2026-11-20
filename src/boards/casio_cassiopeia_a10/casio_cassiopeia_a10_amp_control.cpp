#include "casio_cassiopeia_a10_amp_control.h"

namespace cerfcore {

namespace {

constexpr uint16_t kSpeakerAmp    = 0x0001u;
constexpr uint16_t kMicrophoneAmp = 0x0004u;

}

bool CasioCassiopeiaA10AmpControl::SpeakerOn() const { return (Value() & kSpeakerAmp) != 0u; }

bool CasioCassiopeiaA10AmpControl::MicrophoneOn() const { return (Value() & kMicrophoneAmp) != 0u; }

REGISTER_SERVICE(CasioCassiopeiaA10AmpControl);

}
