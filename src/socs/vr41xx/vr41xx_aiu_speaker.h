#pragma once

#include "../../host/paced_wave_out.h"
#include "vr41xx_aiu_path.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

class Vr41xxAudioSwitch;

class Vr41xxAiuSpeaker : public Vr41xxAiuPath {
public:
    Vr41xxAiuSpeaker(CerfEmulator& emu, Vr41xxAiuPathHost& host);

    void AttachHost();
    void StopHost();

    void SwitchChanged();

    uint16_t SodatReg() const  { return sodat_; }
    uint16_t SdmadatReg() const { return sdmadat_; }
    uint16_t ValidBits() const;
    void     WriteSodat(uint16_t value);
    void     WriteSdmadat(uint16_t value);
    void     WriteValidBits(uint16_t bits);

protected:
    void     Convert() override;
    void     Serve() override;
    bool     WantsDma() const override { return !sdmav_; }
    uint64_t ConversionsToNextChange() const override;
    void     ClearPipeline() override;
    void     OnStart() override;
    void     OnStop() override;
    void     OnRateChanged(uint32_t hz) override;
    void     FinishCatchUp() override;
    void     SavePipeline(StateWriter& w) override;
    void     RestorePipeline(StateReader& r) override;
    void     ResumeHost() override;

private:
    void FlushHost();
    bool Audible() const { return switch_on_ && host_.ConverterEnabled(Vr41xxAiuChannel::Out); }

    Vr41xxAudioSwitch*   switch_    = nullptr;
    PacedWaveOut         paced_;
    std::vector<uint8_t> out_;
    uint16_t             sodat_     = 0x200u;
    uint16_t             sdmadat_   = 0x200u;
    bool                 sodatv_    = false;
    bool                 sdmav_     = false;
    bool                 primed_    = false;
    bool                 switch_on_ = true;
};

}
