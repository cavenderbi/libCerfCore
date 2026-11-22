#pragma once

#include "../core/service.h"

namespace cerfcore {

class MachineVibrator;

class VibrationOutputs : public Service {
public:
    using Service::Service;

    virtual void SetVibrating(const MachineVibrator& vibrator, bool vibrating) = 0;
};

}
