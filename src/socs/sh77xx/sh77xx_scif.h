#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/serial/serial_device_clock_timer.h"
#include "../../peripherals/serial/serial_endpoint_port.h"
#include "../../peripherals/serial/serial_line.h"
#include "sh77xx_intc.h"
#include "sh77xx_scif_fifo.h"
#include "sh77xx_scif_modem_lines.h"
#include "sh77xx_standby_control.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace cerfcore {

inline constexpr uint32_t kSh77xxIrdaBase           = 0x04000140u;
inline constexpr uint32_t kSh77xxScif2Base          = 0x04000150u;
inline constexpr uint32_t kSh77xxScifWindowBytes    = 0x10u;
inline constexpr uint32_t kSh77xxScifTransmitOffset = 0x6u;
inline constexpr uint32_t kSh77xxScifReceiveOffset  = 0xAu;

class DeferredJobs;
class Sh77xxCpg;
class Sh77xxFrqcr;
class Sh77xxScifExternalClock;
class Sh77xxScifPins;
class Sh77xxSerialWiring;

struct Sh77xxScifChannel {
    uint32_t         base;
    const char*      name;
    Sh77xxIntcSource eri;
    Sh77xxIntcSource rxi;
    Sh77xxIntcSource bri;
    Sh77xxIntcSource txi;
    uint8_t          mstp;
    uint32_t         data_pin;
    bool             irda;
    uint8_t          rts_pins;
    bool             modem_control;
    uint32_t         cts_pin;
};

inline constexpr Sh77xxScifChannel kSh77xxIrdaChannel{kSh77xxIrdaBase,
                                                      "IrDA",
                                                      Sh77xxIntcSource::kEri1,
                                                      Sh77xxIntcSource::kRxi1,
                                                      Sh77xxIntcSource::kBri1,
                                                      Sh77xxIntcSource::kTxi1,
                                                      0x01u,
                                                      2u,
                                                      true,
                                                      0x00u,
                                                      false,
                                                      0u};

inline constexpr Sh77xxScifChannel kSh77xxScif2Channel{kSh77xxScif2Base,
                                                       "SCIF2",
                                                       Sh77xxIntcSource::kEri2,
                                                       Sh77xxIntcSource::kRxi2,
                                                       Sh77xxIntcSource::kBri2,
                                                       Sh77xxIntcSource::kTxi2,
                                                       0x02u,
                                                       4u,
                                                       false,
                                                       0x40u,
                                                       true,
                                                       7u};

class Sh77xxScif : public Peripheral, public SerialLine {
public:
    explicit Sh77xxScif(CerfEmulator& emu);
    ~Sh77xxScif() override;

    void OnReady() override;
    void OnShutdown() override;

    uint32_t MmioBase() const override { return Channel().base; }
    uint32_t MmioSize() const override { return kSh77xxScifWindowBytes; }

    uint8_t  ReadByte(uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t value) override;
    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

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

protected:
    virtual const Sh77xxScifChannel& Channel() const = 0;

private:
    void       WriteMode(uint8_t value);
    void       WriteBitRate(uint8_t value);
    void       WriteControl(uint8_t value);
    void       WriteFifoControl(uint8_t value);
    void       WriteStatus(uint16_t value);
    void       WriteTransmitData(uint8_t value);
    uint8_t    ReadReceiveData();
    uint16_t   ReadStatus();
    void       RequireRunning(const char* reg, uint32_t value, uint32_t held) const;
    void       TakeTransmitLocked(std::vector<uint8_t>& out);
    void       PostRelease();
    void       ReleaseHeldTransmit();
    bool       RtsHigh() const;
    bool       IrdaModeLocked() const;
    void       DeliverTx(const std::vector<uint8_t>& bytes, bool powered);
    void       DriveLocked();
    LineConfig LineConfigLocked() const;
    void       FireLineConfig();
    void       RefreshClockStop();
    void       RefreshConnection();
    void       PublishEnables();
    SerialControlLines ControlLines();
    void       RefreshTxLine();
    void       Resync();
    bool       InitializeLocked();
    void       Initialize();
    void       EnterStandby();

    Sh77xxIntc*         intc_   = nullptr;
    Sh77xxScifPins*     pins_   = nullptr;
    Sh77xxStbcr2*       stbcr2_ = nullptr;
    Sh77xxCpg*          cpg_    = nullptr;
    Sh77xxFrqcr*        frqcr_  = nullptr;
    Sh77xxSerialWiring*      wiring_ = nullptr;
    Sh77xxScifExternalClock* clock_  = nullptr;
    DeferredJobs*            jobs_   = nullptr;
    Sh77xxScifModemLines     modem_;

    mutable std::mutex   mu_;
    uint8_t              smr_       = 0u;
    uint8_t              brr_       = 0xFFu;
    uint8_t              scr_       = 0u;
    uint8_t              fcr_       = 0u;
    bool                 stopped_   = false;
    bool                 powered_   = false;
    std::atomic<bool>    tx_low_{false};
    Sh77xxScifFifo       fifo_;
    LineConfig           line_cfg_;

    std::string            tx_line_;
    SerialDeviceClockTimer endpoint_timer_;
    SerialEndpointPort     port_;
};

}
