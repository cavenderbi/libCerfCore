#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/state/state_stream.h>
#include "../../host/host_pcm_queue.h"
#include "../../host/paced_wave_out.h"
#include "../../socs/guest_sample_clock.h"

#include <array>
#include <cstdint>

namespace cerfcore {

class HpJornada540AudioRequestLine;
class Sh77xxDmac;
class Sh77xxPortWiring;

class HpJornada540AudioOut : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;
    void OnShutdown() override;

    void     SetEnabled(bool enabled);
    void     SetFifoReset(bool held);
    void     SetFrameDivisor(uint16_t divisor);
    void     WriteSample(uint16_t word);
    uint16_t WrittenWords() const { return written_; }
    uint16_t PlayedWords() const { return played_; }
    void     PowerOnReset();

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r, bool enabled, bool reset_held);

private:
    static constexpr uint32_t kFifoWords  = 128u;
    static constexpr uint32_t kFrameWords = 2u;

    uint32_t              Fill() const;
    bool                  Requesting() const;
    bool                  Running() const;
    GuestCycleClock::Rate FrameRate() const;
    uint32_t              FrameHz() const;
    bool                  Muted() const;
    uint16_t              Pop();
    void                  ClearFifo();
    void                  DriveRequest();
    void                  Retime();
    void                  BeginStream();
    void                  EndStream();
    void                  Frame();

    GuestSampleClock                 frames_{emu_};
    GuestSampleClock                 flush_{emu_};
    PacedWaveOut                     paced_;
    HostPcmQueue                     out_;
    Sh77xxDmac*                      dmac_    = nullptr;
    Sh77xxPortWiring*                wiring_  = nullptr;
    HpJornada540AudioRequestLine*    request_ = nullptr;
    uint64_t                         ckio_hz_ = 0u;
    std::array<uint16_t, kFifoWords> fifo_{};
    uint16_t                         written_    = 0u;
    uint16_t                         played_     = 0u;
    uint16_t                         divisor_    = 0u;
    bool                             enabled_    = false;
    bool                             reset_held_ = false;
    bool                             running_    = false;
};

}
