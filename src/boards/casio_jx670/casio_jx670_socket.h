#pragma once

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

class CasioJx670Socket {
public:
    void PowerOnRegisters();

    bool TryReadHalf (uint32_t off, uint16_t& out) const;
    bool TryReadWord (uint32_t off, uint32_t& out) const;
    bool TryWriteHalf(uint32_t off, uint16_t  value);
    bool TryWriteWord(uint32_t off, uint32_t  value);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    uint32_t strap8000_       = 0;
    uint32_t socket_8014_     = 0;
    uint32_t cfg_8018_        = 0;
    uint32_t socket_ctrl_ac8_ = 0;
    uint32_t socket_a038_     = 0;
};

}
