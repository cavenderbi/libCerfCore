#pragma once

#include <cerfcore/core/service.h>

#include "../host/wave_in_fifo.h"

#include <cstdint>

namespace cerfcore {

class AudioActivity;

class Sh77xxAdcMicrophone : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override = 0;
    void OnReady() override;
    void OnShutdown() override;

    void     SetRate(uint32_t rate_hz);
    uint16_t Convert();

private:
    WaveInFifo     mic_;
    AudioActivity* activity_ = nullptr;
};

}
