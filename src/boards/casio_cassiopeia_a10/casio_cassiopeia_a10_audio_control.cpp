#include "casio_cassiopeia_a10_audio_control.h"

namespace cerfcore {

namespace {

constexpr uint16_t kMicrophonePower = 0x0001u;

}

bool CasioCassiopeiaA10AudioControl::MicrophoneOn() const { return (Value() & kMicrophonePower) != 0u; }

REGISTER_SERVICE(CasioCassiopeiaA10AudioControl);

}
