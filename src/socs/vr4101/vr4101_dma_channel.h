#pragma once

#include <cstdint>

namespace cerfcore {

enum class Vr4101DmaChannel : uint32_t {
    kPiu   = 0u,
    kSiuRx = 1u,
    kSiuTx = 2u,
    kAiu   = 3u,
    kKiu   = 4u,
};

}
