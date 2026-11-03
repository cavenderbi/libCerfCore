#pragma once

#include <cstdint>

namespace cerfcore {

class SerialLineTimer {
public:
    virtual ~SerialLineTimer() = default;

    virtual void ArmLineTimer(int64_t delay_ns) = 0;
    virtual void DisarmLineTimer() = 0;
};

}
