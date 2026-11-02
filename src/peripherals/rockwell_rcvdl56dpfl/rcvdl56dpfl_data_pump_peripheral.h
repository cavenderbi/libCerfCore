#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../execution/guest_cycle_clock.h"
#include "../../socs/rated_tick_count.h"
#include "rcvdl56dpfl_data_pump.h"
#include "rcvdl56dpfl_data_pump_host.h"

#include <cstdint>

namespace cerfcore {

class Rcvdl56dpflDataPumpPeripheral : public Peripheral, private Rcvdl56dpflDataPumpHost {
public:
    using Peripheral::Peripheral;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

protected:
    void    StartDataPump();
    uint8_t ReadPumpRegister(uint32_t reg);
    void    WritePumpRegister(uint32_t reg, uint8_t value);
    void    SetResetPin(bool high);

    virtual void DriveIrq(bool asserted) = 0;

private:
    void     SetIrqLevel(bool asserted) override;
    void     SetBitRate(uint32_t bits_per_second) override;
    uint64_t BitTickNow() override;
    uint64_t BitTicksElapsed() override;
    void     ArmTimer(uint64_t tick) override;
    void     RescaleBitClock();

    GuestCycleClock*        clock_ = nullptr;
    GuestCycleClock::Event* event_ = nullptr;
    RatedTickCount          bit_ticks_{emu_};
    GuestCycleClock::Rate   bit_rate_{};
    bool                    timed_ = false;
    Rcvdl56dpflDataPump     pump_{*this};
};

}
