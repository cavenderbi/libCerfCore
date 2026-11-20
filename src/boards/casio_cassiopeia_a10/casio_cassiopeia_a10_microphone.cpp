#include "casio_cassiopeia_a10_microphone.h"

#include <algorithm>
#include <cstddef>

namespace cerfcore {

void CasioCassiopeiaA10Microphone::Silence(uint8_t* slots, uint32_t count, uint32_t slot_bytes) {
    for (uint32_t i = 0; i < count; ++i) {
        std::fill_n(slots + static_cast<size_t>(i) * slot_bytes, kSampleBytes, uint8_t{0u});
    }
}

}
