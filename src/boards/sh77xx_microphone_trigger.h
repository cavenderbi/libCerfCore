#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Sh77xxAdc;
class Sh77xxAdcMicrophone;

class Sh77xxMicrophoneTrigger : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void Pulse(uint32_t rate_hz);

private:
    Sh77xxAdc*           adc_        = nullptr;
    Sh77xxAdcMicrophone* microphone_ = nullptr;
};

}
