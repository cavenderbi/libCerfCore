#pragma once

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

struct Pci1225ExcaChipWrite {
    const char* refused        = nullptr;
    bool        software_csc   = false;
    bool        global_changed = false;
};

class Pci1225ExcaChipRegisters {
public:
    static bool Owns(uint8_t idx);

    uint8_t Read(uint8_t idx, bool vs1_grounded, bool vs2_grounded, uint8_t global_control) const;
    Pci1225ExcaChipWrite Write(uint8_t idx, uint8_t value, bool id_writable, uint8_t* global_control);

    void Reset();

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    static constexpr uint8_t kIdRevisionReset = 0x84u;

    uint8_t id_revision_ = kIdRevisionReset;
    uint8_t cd_control_  = 0u;
};

}
