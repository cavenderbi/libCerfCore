#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include "../../execution/guest_cycle_clock.h"
#include "sh77xx_dmac.h"
#include "sh77xx_intc.h"
#include "sh77xx_peripheral_clock_ticks.h"
#include "sh77xx_standby_control.h"

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kSh77xxAdcBase        = 0x04000080u;
inline constexpr uint32_t kSh77xxAdcDataBytes   = 0x10u;
inline constexpr uint32_t kSh77xxAdcWindowBytes = 0x14u;

class Sh77xxAnalogInputs;

class Sh77xxAdc : public Peripheral {
public:
    using Peripheral::Peripheral;

    void OnReady() override;

    uint32_t MmioBase() const override { return kSh77xxAdcBase; }
    uint32_t MmioSize() const override { return kSh77xxAdcWindowBytes; }

    uint8_t ReadByte(uint32_t addr) override;
    void    WriteByte(uint32_t addr, uint8_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

    void ExternalTrigger();

protected:
    struct ControlModel {
        uint8_t stored;
        uint8_t read_ones;
        uint8_t write_zero;
        uint8_t scan;
        bool    multi_scans;
    };

    virtual ControlModel Control() const          = 0;
    virtual uint8_t      Stbcr2ModuleStop() const = 0;

private:
    static constexpr uint32_t kDataRegisters = 4u;

    uint8_t  ReadData(uint32_t offset);
    uint8_t  ReadControlStatus();
    void     WriteControlStatus(uint8_t value);
    void     Start();
    void     Convert();
    void     RequireClock() const;
    bool     Scans() const;
    uint64_t PassTicks() const;
    void     Sync();
    void     Arm();
    void     StopScan();
    void     ClearEndFlag();
    bool     Interrupting() const;
    void     Drive();
    void     Initialize();

    Sh77xxIntc*                intc_   = nullptr;
    Sh77xxDmac*                dmac_   = nullptr;
    Sh77xxStbcr2*              stbcr2_ = nullptr;
    Sh77xxAnalogInputs*        inputs_ = nullptr;
    GuestCycleClock*           clock_  = nullptr;
    GuestCycleClock::Event*    event_  = nullptr;
    Sh77xxPeripheralClockTicks pclk_{emu_};
    uint16_t                   data_[kDataRegisters]{};
    uint8_t                    control_     = 0u;
    uint8_t                    adcr_        = 0u;
    uint8_t                    temp_        = 0u;
    bool                       end_         = false;
    bool                       end_read_    = false;
    bool                       temp_loaded_ = false;
    bool                       scanning_    = false;
    uint64_t                   next_pass_   = 0u;
    uint64_t                   restored_ticks_to_pass_ = 0u;
};

}
