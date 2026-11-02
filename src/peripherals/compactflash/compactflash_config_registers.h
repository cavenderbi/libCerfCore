#pragma once

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

class CompactFlashConfigRegisters {
public:
    static bool Decodes(uint32_t offset);

    void Reset();

    uint8_t ConfigIndex() const;
    bool    IoInterface() const;
    bool    LevelInterrupts() const;
    bool    StatusChangeAsserted() const;
    bool    SoftResetAsserted() const;
    bool    PowerDownRequested() const;
    uint8_t DriveNumber() const;
    void    NoteReadyChange();

    uint8_t Read(uint32_t offset, bool interrupt_pending, bool ready) const;
    uint8_t Write(uint32_t offset, uint8_t value);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    uint8_t cor_       = 0u;
    uint8_t ccsr_      = 0u;
    uint8_t changed_   = 0u;
    uint8_t scr_drive_ = 0u;
};

}
