#include "casio_cassiopeia_a10_microphone.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/instance_threads.h>
#include "../../host/audio_activity.h"
#include "../../host/wave_in_fifo.h"
#include "casio_cassiopeia_a10_amp_control.h"
#include "casio_cassiopeia_a10_audio_control.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

namespace cerfcore {

namespace {

constexpr size_t   kMicFifoBytes = 65536u;
constexpr uint16_t kChannels     = 1u;
constexpr uint16_t kSampleBits   = 16u;

constexpr char kLogTag[] = "CasioCassiopeiaA10Microphone";

class CasioCassiopeiaA10HostMicrophone : public CasioCassiopeiaA10Microphone {
public:
    using CasioCassiopeiaA10Microphone::CasioCassiopeiaA10Microphone;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override {
        activity_ = &emu_.Get<AudioActivity>();
        amps_     = &emu_.Get<CasioCassiopeiaA10AmpControl>();
        control_  = &emu_.Get<CasioCassiopeiaA10AudioControl>();
        mic_.Start(emu_.Get<InstanceThreads>(), emu_.TryGet<AudioInputs>(), kLogTag, kMicFifoBytes);
    }

    void OnShutdown() override { mic_.Stop(); }

    void Open(uint32_t rate_hz) override {
        mic_.Clear();
        mic_.Request(rate_hz, kChannels, kSampleBits);
    }

    void Fill(uint8_t* slots, uint32_t count, uint32_t slot_bytes) override {
        const bool heard = mic_.TakeIntoSlots(slots, count, kSampleBytes, slot_bytes) != 0u;
        if (!Powered()) {
            Silence(slots, count, slot_bytes);
            return;
        }
        if (heard) activity_->MarkRx();
    }

private:
    bool Powered() const { return control_->MicrophoneOn() && amps_->MicrophoneOn(); }

    WaveInFifo                      mic_;
    AudioActivity*                  activity_ = nullptr;
    CasioCassiopeiaA10AmpControl*   amps_     = nullptr;
    CasioCassiopeiaA10AudioControl* control_  = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10HostMicrophone, CasioCassiopeiaA10Microphone);

}
