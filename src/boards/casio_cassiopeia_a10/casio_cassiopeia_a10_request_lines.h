#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

enum class CasioCassiopeiaA10Request : uint8_t {
    kKeyMatrix,
    kPen,
    kOutsideKey,
    kAudioSwitched,
    kAudioEnded,
};

class CasioCassiopeiaA10RequestLines : public Service {
public:
    using Service::Service;

    virtual void Drive(CasioCassiopeiaA10Request request, bool asserted) = 0;
};

}
