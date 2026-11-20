#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/serial/serial_device_clock_timer.h"
#include "../../peripherals/serial/serial_endpoint_port.h"
#include "../../peripherals/serial/serial_line.h"
#include "sh77xx_sci_rx_line.h"
#include "sh77xx_sci_tx_line.h"
#include "sh77xx_standby_control.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace cerfcore {

class Sh77xxCpg;
class Sh77xxFrqcr;
class Sh77xxIntc;
class Sh77xxSciPort;
class Sh77xxSerialWiring;

class Sh77xxSci : public Peripheral, public SerialLine {
public:
    explicit Sh77xxSci(CerfEmulator& emu);

    void OnReady() override;
    void OnShutdown() override;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

    uint8_t ReadByte(uint32_t addr) override;
    void    WriteByte(uint32_t addr, uint8_t value) override;

    void       PushRx(const uint8_t* data, size_t n) override;
    bool       RxEmpty() const override;
    void       SetRxDrainCallback(RxDrainFn cb) override;
    void       SetModemInputs(bool cts, bool dsr, bool ri, bool dcd) override;
    LineConfig GetLineConfig() const override;
    void       SetLineConfigCallback(LineConfigFn cb) override;
    void       SetEndpoint(SerialEndpoint* ep) override;
    void       ArmEndpointTimer(int64_t delay_ns) override;
    void       DisarmEndpointTimer() override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

private:
    void               RequireRunning(const char* access, uint32_t addr, uint32_t value);
    bool               Transmitting() const;
    SerialControlLines ControlLines();
    uint8_t            StatusLocked() const;
    void               Reformat(uint8_t smr);
    void               WriteControl(uint32_t addr, uint8_t value);
    void               WriteTransmit(uint8_t value);
    void               WriteStatus(uint8_t value);
    uint8_t            ReadReceive();
    void               Deliver(const std::vector<uint8_t>& bytes);
    void               OnHostRequest();
    void               SettleLocked();
    void               ForgetReadsLocked(uint8_t risen);
    void               OnLineDue();
    void               DriveLocked();
    LineConfig         LineConfigLocked() const;
    void               FireLineConfig();
    void               SettleAndDeliver();
    void               Initialize();
    void               InitializeAndNotify();
    void               RefreshClockStop();
    void               RefreshConnection();
    void               OnWiringChanged();
    void               Check(bool scaled);

    Sh77xxIntc*         intc_   = nullptr;
    Sh77xxStbcr*        stbcr_  = nullptr;
    Sh77xxCpg*          cpg_    = nullptr;
    Sh77xxFrqcr*        frqcr_  = nullptr;
    Sh77xxSerialWiring* wiring_ = nullptr;
    Sh77xxSciPort*      scsptr_ = nullptr;

    mutable std::mutex mu_;
    uint8_t            smr_        = 0u;
    uint8_t            brr_        = 0u;
    uint8_t            scr_        = 0u;
    uint8_t            mpbt_       = 0u;
    uint8_t            read_flags_ = 0u;
    bool               stopped_    = false;
    bool               powered_    = false;
    LineConfig         line_cfg_;
    std::string        debug_text_;
    Sh77xxSciRxLine    rx_line_;
    Sh77xxSciTxLine    tx_line_;

    SerialDeviceClockTimer endpoint_timer_;
    SerialEndpointPort     port_;
};

}
