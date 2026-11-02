#include "compactflash_register_map.h"

namespace cerfcore {

namespace {

constexpr uint8_t kConfigMemoryMapped = 0u;
constexpr uint8_t kConfigContiguousIo = 1u;
constexpr uint8_t kConfigPrimaryIo    = 2u;
constexpr uint8_t kConfigSecondaryIo  = 3u;

constexpr uint32_t kPrimaryCommandBase   = 0x1F0u;
constexpr uint32_t kPrimaryControlBase   = 0x3F6u;
constexpr uint32_t kSecondaryCommandBase = 0x170u;
constexpr uint32_t kSecondaryControlBase = 0x376u;
constexpr uint32_t kTenBitIoMask         = 0x3FFu;

constexpr uint32_t kRegisterBlockMask   = 0x0Fu;
constexpr uint32_t kMemoryRegisterSpace = 0x800u;
constexpr uint32_t kMemoryDataWindow    = 0x400u;

int BlockRegister(uint32_t offset) {
    const int reg = static_cast<int>(offset & kRegisterBlockMask);
    if (reg <= CompactFlashReg::kDupOddData || reg >= CompactFlashReg::kDupError)
        return reg;
    return CompactFlashReg::kNone;
}

int LegacyIoRegister(uint32_t offset, uint32_t command_base, uint32_t control_base) {
    const uint32_t off = offset & kTenBitIoMask;
    if (off >= command_base && off <= command_base + CompactFlashReg::kCommand)
        return static_cast<int>(off - command_base);
    if (off == control_base)
        return CompactFlashReg::kDevCtl;
    if (off == control_base + 1u)
        return CompactFlashReg::kDriveAddress;
    return CompactFlashReg::kNone;
}

int MemoryMappedRegister(uint32_t offset) {
    if (offset >= kMemoryRegisterSpace)
        return CompactFlashReg::kNone;
    if ((offset & kMemoryDataWindow) == 0u)
        return BlockRegister(offset);
    return (offset & 1u) != 0u ? CompactFlashReg::kDupOddData : CompactFlashReg::kDupEvenData;
}

}

int CompactFlashRegisterAt(uint8_t config_index, CompactFlashSpace space, uint32_t offset) {
    if (space == CompactFlashSpace::kCommonMemory)
        return config_index == kConfigMemoryMapped ? MemoryMappedRegister(offset) : CompactFlashReg::kNone;
    switch (config_index) {
        case kConfigMemoryMapped:
            return CompactFlashReg::kNone;
        case kConfigContiguousIo:
            return BlockRegister(offset);
        case kConfigPrimaryIo:
            return LegacyIoRegister(offset, kPrimaryCommandBase, kPrimaryControlBase);
        case kConfigSecondaryIo:
            return LegacyIoRegister(offset, kSecondaryCommandBase, kSecondaryControlBase);
        default:
            return CompactFlashReg::kUnmapped;
    }
}

bool CompactFlashWordDataRegister(int reg) {
    return reg == CompactFlashReg::kData || reg == CompactFlashReg::kDupEvenData;
}

}
