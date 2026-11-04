#pragma once

#include <cstdint>

namespace cerfcore {

struct Rcvdl56dpflSelfTest {
    uint16_t ram1_checksum;
    uint16_t ram2_checksum;
    uint16_t rom1_checksum;
    uint16_t rom2_checksum;
    uint16_t timer_rom_ram;
    uint16_t controller_part_number;
    uint16_t controller_revision;
    uint16_t ec_checksum;
    uint16_t multiplier_checksum;
    uint16_t dsp_ram_checksum;
    uint16_t dsp_rom_checksum;
    uint16_t dsp_part_number;
    uint16_t dsp_revision;
};

inline constexpr Rcvdl56dpflSelfTest kRcvdl56dpflTypicalSelfTest = {
    0xEA3Cu, 0x5536u, 0x5F4Cu, 0x3832u, 0x0801u, 0x3436u, 0x4241u,
    0x020Cu, 0x46EEu, 0xB2A6u, 0x3832u, 0x3436u, 0x2041u,
};

}
