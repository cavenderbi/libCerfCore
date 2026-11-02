#pragma once

#include <cstdint>

namespace cerfcore {

struct CompactFlashReg {
    enum : int {
        kUnmapped     = -2,
        kNone         = -1,
        kData         = 0x0,
        kError        = 0x1,
        kFeature      = 0x1,
        kSectCnt      = 0x2,
        kSectNum      = 0x3,
        kCylLow       = 0x4,
        kCylHigh      = 0x5,
        kDrvHead      = 0x6,
        kStatus       = 0x7,
        kCommand      = 0x7,
        kDupEvenData  = 0x8,
        kDupOddData   = 0x9,
        kDupError     = 0xD,
        kDupFeature   = 0xD,
        kDevCtl       = 0xE,
        kDriveAddress = 0xF,
    };
};

enum class CompactFlashSpace { kIo, kCommonMemory };

int CompactFlashRegisterAt(uint8_t config_index, CompactFlashSpace space, uint32_t offset);

bool CompactFlashWordDataRegister(int reg);

}
