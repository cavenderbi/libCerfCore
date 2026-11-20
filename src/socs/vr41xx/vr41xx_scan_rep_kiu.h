#pragma once

#include "vr41xx_kiu.h"
#include "vr41xx_port_giu.h"

#include <atomic>
#include <cstdint>
#include <optional>

namespace cerfcore {

class Vr41xxScanRepKiu : public Vr41xxKiu {
public:
    using Vr41xxKiu::Vr41xxKiu;

    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

protected:
    void     AttachFrontEnd() override;
    GuestCycleClock::Rate ScanTickRate() override;
    uint16_t PiodLocked() const override;
    void     ScanStoredLocked() override;
    void     ResetFrontEndLocked() override {}
    void     PinsChangedLocked() override;
    void     FlushPinsChange() override;

private:
    struct KscanDrive {
        uint16_t released  = 0;
        uint16_t undefined = 0;
        uint16_t driven    = 0;
        uint16_t high      = 0;
    };
    KscanDrive              KscanDriveMasksLocked() const;
    std::optional<PinDrive> KscanDriveLocked(int gpio);
    uint64_t                DriveSignatureLocked() const;

    Vr41xxPortGiu*    giu_ = nullptr;
    uint64_t          drive_signature_ = 0;
    std::atomic<bool> drive_changed_{false};
};

}
