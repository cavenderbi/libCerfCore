#pragma once

#include <cerfcore/core/service.h>
#include "../../host/host_pcm_queue.h"
#include "../../host/paced_wave_out.h"

#include <cstdint>

namespace cerfcore {

class Pr31x00SibCodec;

class Pr31x00SibSpeaker : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;
    void OnShutdown() override;

    void SetFormat(uint32_t hz, uint16_t channels);
    void Begin();
    void Finish();
    void Abort();
    void AddWord(uint32_t hold, bool sixteen_bit);
    void Flush();

private:
    Pr31x00SibCodec* codec_ = nullptr;
    PacedWaveOut     paced_;
    HostPcmQueue     buf_;
};

}
