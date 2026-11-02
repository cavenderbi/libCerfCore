#pragma once

#include <cstdint>

namespace cerfcore {

enum class PcmciaIoWidth : uint8_t {
    kEightBit,
    kSixteenBit,
    kCardIois16,
};

}
