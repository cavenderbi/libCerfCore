#pragma once

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

class RicohRf5c396ChipRegisters {
public:
    static constexpr uint8_t kRegCardDetectControl = 0x16u;
    static constexpr uint8_t kRegGlobalControl     = 0x1Eu;
    static constexpr uint8_t kRegModeControl3      = 0x3Bu;

    static constexpr uint8_t kCdcSoftwareCardDetect = 0x20u;
    static constexpr uint8_t kCdcGpiEnable          = 0x04u;
    static constexpr uint8_t kCdcConfigurationReset = 0x02u;

    static bool Owns(uint8_t sub);

    bool        Read(int bank, uint8_t sub, uint8_t* value) const;
    const char* Write(int bank, uint8_t sub, uint8_t value);
    void        Reset();

    bool     ExplicitStatusClear() const;
    bool     LevelInterrupts() const;
    bool     CardDetectResume(int bank) const;
    bool     GpiEnabled(int bank) const;
    bool     GpiInterruptOnRise(int bank) const;
    bool     ConfigurationResetOnRemoval(int bank) const;
    bool     IreqActiveHigh(int bank) const;
    bool     InterfaceDisabled(int bank) const;
    bool     InpackEnabled(int bank) const;
    bool     DisablesDataBit7(int bank, uint32_t isa_io) const;
    uint32_t CardAddressHighLines(int bank, uint32_t card_addr) const;

    void SaveBank(StateWriter& w, int bank) const;
    void RestoreBank(StateReader& r, int bank);
    void SaveGlobal(StateWriter& w) const;
    void RestoreGlobal(StateReader& r);

private:
    uint8_t card_detect_control_[2] = { 0u, 0u };
    uint8_t mode_control1_[2]       = { 0u, 0u };
    uint8_t mode_control2_[2]       = { 0u, 0u };
    uint8_t mode_control3_[2]       = { 0u, 0u };
    uint8_t global_control_         = 0u;
};

}
