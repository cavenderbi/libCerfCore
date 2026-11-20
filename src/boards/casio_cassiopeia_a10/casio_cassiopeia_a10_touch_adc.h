#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/touch/host_pen_target.h"

#include <cstdint>

namespace cerfcore {

class CasioCassiopeiaA10RequestLines;

class CasioCassiopeiaA10TouchAdc : public Peripheral, public HostPenTarget {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

    void ApplyPen(const HostPenInput::Pen& pen) override;
    void ResetPen() override;

private:
    static constexpr uint32_t kSamples     = 3u;
    static constexpr uint32_t kSetupFields = 3u;

    bool PenDetected() const;
    void DrivePenLine();
    void StartSingle(uint32_t addr, uint16_t channel);
    void StartSequence(uint32_t addr, uint16_t plate);
    void WriteSetup(uint32_t addr, uint16_t value);
    void Reset();

    CasioCassiopeiaA10RequestLines* requests_            = nullptr;
    bool                            host_pen_down_       = false;
    uint16_t                        power_state_         = 0u;
    uint16_t                        status_              = 0u;
    uint16_t                        conversion_select_   = 0u;
    bool                            conversion_selected_ = false;
    uint8_t                         setup_written_       = 0u;
    uint16_t                        setup_[kSetupFields]{};
    bool                            single_ready_        = false;
    uint16_t                        single_result_       = 0u;
    bool                            samples_ready_       = false;
    uint16_t                        samples_[kSamples]{};
};

}
