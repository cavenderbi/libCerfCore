#pragma once

#include "../pcmcia/pcmcia_card.h"
#include <cerfcore/host/external_display.h>
#include "vga_controller.h"

#include <cstdint>
#include <memory>

namespace cerfcore {

class HpPalmtopVgaCard : public PcmciaCard {
public:
    static constexpr const char* kDisplayName = "HP Palmtop VGA (F1252A)";

    explicit HpPalmtopVgaCard(CerfEmulator& emu);
    ~HpPalmtopVgaCard() override;

    std::string DisplayName() const override { return kDisplayName; }
    PcmciaVoltageSense VoltageSense() const override { return { false, false, false }; }
    bool HoldsReadyBusyInReset() const override { return false; }
    const char* IconResource() const override { return "pcmcia_enabled_vga"; }

    void OnInserted() override;
    void OnShutdown() override;

    void PowerOn () override;

    const char* SaveId() const override { return "hpvga"; }
    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PowerOff() override;

    uint8_t  ReadAttribute8 (uint32_t offset)                override;
    void     WriteAttribute8(uint32_t offset, uint8_t value) override;

    uint8_t  ReadCommon8  (uint32_t offset)                  override;
    uint16_t ReadCommon16 (uint32_t offset)                  override;
    void     WriteCommon8 (uint32_t offset, uint8_t  value)  override;
    void     WriteCommon16(uint32_t offset, uint16_t value)  override;

    uint8_t  ReadIo8  (uint32_t offset)                      override;
    uint16_t ReadIo16 (uint32_t offset)                      override;
    void     WriteIo8 (uint32_t offset, uint8_t  value)      override;
    void     WriteIo16(uint32_t offset, uint16_t value)      override;
    bool     Iois16   (uint32_t)                             override { return true; }
    bool     InputAcknowledge(uint32_t)                      override { return false; }

private:
    void SyncWindowToMode();

    static constexpr uint32_t kRegEnd    = 0x0046E9u;
    static constexpr uint32_t kMagicBase = 0x0C0000u;
    static constexpr uint32_t kMagicLen  = 8u;

    [[noreturn]] void HaltUnmapped(const char* space, const char* access, uint32_t offset);

    VgaController controller_;
    std::unique_ptr<ExternalDisplay> window_;

    uint32_t last_w_ = 0, last_h_ = 0;
};

}
