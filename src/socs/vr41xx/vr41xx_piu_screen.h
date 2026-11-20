#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Vr41xxPiuScreen : public Service {
public:
    using Service::Service;

    virtual uint16_t ScreenRawXMax() const { return 1023u; }

    virtual bool HasTouchscreen() const { return true; }
};

}
