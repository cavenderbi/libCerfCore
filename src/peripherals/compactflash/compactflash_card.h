#pragma once

#include "../pcmcia/pcmcia_card.h"
#include "compactflash_ata.h"
#include "compactflash_config_registers.h"
#include "compactflash_register_map.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace cerfcore {

class CompactFlashCard : public PcmciaCard {
public:
    CompactFlashCard(CerfEmulator& emu, std::string image_path);
    ~CompactFlashCard() override;

    std::string DisplayName() const override;
    PcmciaVoltageSense VoltageSense() const override { return { true, false, false }; }
    bool HoldsReadyBusyInReset() const override { return true; }
    std::string TooltipDetail() const override;
    const char* IconResource() const override { return "pcmcia_enabled_cf"; }

    void PowerOn () override;
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
    bool     Iois16   (uint32_t offset)                      override;
    bool     InputAcknowledge(uint32_t offset)               override;

    const char* SaveId() const override { return "cf"; }
    std::string SaveBinding() const override { return image_path_; }
    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

private:
    int      RegisterAtLocked(CompactFlashSpace space, uint32_t offset) const;
    uint8_t  ReadRegisterLocked(CompactFlashSpace space, uint32_t offset);
    void     WriteRegisterLocked(CompactFlashSpace space, uint32_t offset, uint8_t value);
    uint16_t ReadWordLocked(CompactFlashSpace space, uint32_t offset);
    void     WriteWordLocked(CompactFlashSpace space, uint32_t offset, uint16_t value);
    void UpdateIreqLocked();

    const std::string image_path_;
    const std::string image_name_;

    mutable std::mutex mtx_;

    CompactFlashAta             ata_;
    CompactFlashConfigRegisters config_;
    bool                        ireq_driven_   = false;
    bool                        pulse_request_ = false;
    bool                        last_ready_    = true;
};

}
