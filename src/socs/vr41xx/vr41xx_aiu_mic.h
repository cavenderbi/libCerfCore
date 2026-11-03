#pragma once

#include "../../platform/wave_in_fifo.h"
#include "vr41xx_aiu_path.h"

#include <cstdint>

namespace cerfcore {

class Vr41xxAudioSwitch;

class Vr41xxAiuMic : public Vr41xxAiuPath {
public:
    Vr41xxAiuMic(CerfEmulator& emu, Vr41xxAiuPathHost& host);

    void AttachHost();
    void StopHost();

    void SwitchChanged();

    uint16_t MidatReg() const   { return midat_; }
    uint16_t MdmadatReg() const { return mdmadat_; }
    uint16_t ValidBits() const;
    void     WriteMidat(uint16_t value);
    void     WriteMdmadat(uint16_t value);
    void     WriteValidBits(uint16_t bits);

protected:
    void     Convert() override;
    void     Serve() override;
    bool     WantsDma() const override { return mdmav_; }
    uint64_t ConversionsToNextChange() const override;
    void     ClearPipeline() override;
    void     OnStart() override;
    void     OnStop() override;
    void     OnRateChanged(uint32_t hz) override;
    void     FinishCatchUp() override {}
    void     SavePipeline(StateWriter& w) override;
    void     RestorePipeline(StateReader& r) override;
    void     ResumeHost() override;

private:
    uint16_t NextSample();
    void     MoveToDma();

    Vr41xxAudioSwitch* switch_  = nullptr;
    WaveInFifo         mic_;
    uint16_t           midat_   = 0x200u;
    uint16_t           mdmadat_ = 0x200u;
    bool               midatv_  = false;
    bool               mdmav_   = false;
    bool               mic_on_  = true;
};

}
