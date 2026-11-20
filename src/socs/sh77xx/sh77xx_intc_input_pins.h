#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/state/state_stream.h>
#include "sh77xx_intc_model.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

class Sh77xxIntcInputPins : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    bool     High(uint32_t n) const;
    void     SetNet(uint32_t bit, bool high);
    void     SetDrive(uint32_t pins, uint32_t high);
    bool     Apply(uint16_t icr1, uint32_t irl_pins, bool resetting);
    bool     IrqRequest(uint16_t icr1, uint32_t n) const;
    uint32_t PintRequests(uint16_t icr2, uint16_t pinter) const;
    uint32_t IrlCode(bool irls_in_use) const;
    uint8_t  ReadIrr0(uint16_t icr1, uint16_t icr2, uint16_t pinter, uint32_t absent_pints);
    void     ClearIrr0(uint8_t value);
    void     ResetLatches();
    void     SaveState(StateWriter& w) const;
    void     RestoreState(StateReader& r);

private:
    static uint32_t Sense(uint16_t icr1, uint32_t n);

    bool     HighLocked(uint32_t n) const;
    bool     IrqRequestLocked(uint16_t icr1, uint32_t n) const;
    uint32_t PintRequestsLocked(uint16_t icr2, uint16_t pinter) const;
    uint32_t Settled() const;

    mutable std::mutex mtx_;
    uint32_t           high_           = sh77xx_intc::kAllPinsHigh;
    uint32_t           net_high_       = sh77xx_intc::kAllPinsHigh;
    uint32_t           drive_pins_     = 0u;
    uint32_t           drive_high_     = 0u;
    uint32_t           irq_latch_      = 0u;
    uint32_t           irr0_read_ones_ = 0u;
};

}
