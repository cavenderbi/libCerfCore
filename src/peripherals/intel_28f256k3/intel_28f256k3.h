#pragma once

#include "../intel_command_set_flash.h"

#include <cstdint>

namespace cerfcore {

class Intel28F256K3 : public IntelCommandSetFlash {
public:
    using IntelCommandSetFlash::IntelCommandSetFlash;

    void OnReady() override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

protected:
    uint16_t Manufacturer() const override { return 0x0089u; }
    uint16_t Device()       const override { return 0x8803u; }
    uint32_t ChipEraseBlockBytes() const override { return 0x20000u; }
    uint32_t DeviceWidth() const override { return 2u; }

    void ResetDevice() override { PowerUp(); }

    bool WriteBufferRefused() const override {
        return (StatusErrors() & (kSrEraseErr | kSrProgErr)) == (kSrEraseErr | kSrProgErr);
    }
    bool     WriteBufferUsesXsr() const override { return false; }
    uint32_t WriteBufferWords(uint32_t addr, uint32_t value) override;

    bool PartCommand(uint32_t addr, uint8_t cmd) override;
    void LockSetupCommand(uint32_t addr, uint8_t cmd) override;
    void PresentIdentifier(uint32_t addr) override;

private:
    static constexpr uint16_t kRcrReset        = 0xFFC7u;
    static constexpr uint32_t kBufferCountMask = 0x1Fu;

    void PowerUp();
    uint32_t IdentifierWord(uint32_t word) const { return MmioBase() + word * BusBytes(); }

    uint16_t rcr_ = kRcrReset;
};

}
