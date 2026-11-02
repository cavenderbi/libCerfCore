#include "compactflash_identify.h"

#include <cerfcore/core/byte_order.h>

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace cerfcore {

namespace {

constexpr uint16_t kGeneralConfiguration = 0x848Au;
constexpr uint16_t kEccByteCount         = 0x0004u;
constexpr uint16_t kMultipleSectorCount  = 0x8001u;
constexpr uint16_t kMultipleBlockSectors = 0x0001u;
constexpr uint16_t kCommandSetsSupported  = 0x7008u;
constexpr uint16_t kCommandSetsCfa        = 0x0004u;
constexpr uint16_t kCommandSetsValid      = 0x4000u;
constexpr uint16_t kCommandSetsSupported2 = kCommandSetsValid | kCommandSetsCfa;
constexpr uint16_t kCapabilitiesLba      = 0x0200u;
constexpr uint16_t kPioTimingMode2       = 0x0200u;
constexpr uint16_t kCurrentChsValid      = 0x0001u;
constexpr uint16_t kMultipleSettingValid = 0x0100u;
constexpr uint32_t kLba28Limit           = 0x0FFFFFFFu;

constexpr std::size_t kWordGeneralConfig  = 0;
constexpr std::size_t kWordCylinders      = 1;
constexpr std::size_t kWordHeads          = 3;
constexpr std::size_t kWordSectors        = 6;
constexpr std::size_t kWordCardSectorsMsw = 7;
constexpr std::size_t kWordCardSectorsLsw = 8;
constexpr std::size_t kWordSerial         = 10;
constexpr std::size_t kWordEccCount       = 22;
constexpr std::size_t kWordFirmware       = 23;
constexpr std::size_t kWordModel          = 27;
constexpr std::size_t kWordMultipleCount  = 47;
constexpr std::size_t kWordCapabilities   = 49;
constexpr std::size_t kWordPioTiming      = 51;
constexpr std::size_t kWordValidity       = 53;
constexpr std::size_t kWordCurCylinders   = 54;
constexpr std::size_t kWordCurHeads       = 55;
constexpr std::size_t kWordCurSectors     = 56;
constexpr std::size_t kWordCurCapacityLsw = 57;
constexpr std::size_t kWordCurCapacityMsw = 58;
constexpr std::size_t kWordMultipleSet    = 59;
constexpr std::size_t kWordLbaSectorsLsw  = 60;
constexpr std::size_t kWordLbaSectorsMsw  = 61;
constexpr std::size_t kWordCommandSetsSupported  = 82;
constexpr std::size_t kWordCommandSetsSupported2 = 83;
constexpr std::size_t kWordCommandSetsSupported3 = 84;
constexpr std::size_t kWordCommandSetsEnabled    = 85;
constexpr std::size_t kWordCommandSetsEnabled2   = 86;
constexpr std::size_t kWordCommandSetsEnabled3   = 87;

constexpr std::size_t kSerialBytes   = 20;
constexpr std::size_t kFirmwareBytes = 8;
constexpr std::size_t kModelBytes    = 40;

void PutWord(std::array<uint8_t, 512>& block, std::size_t word, uint16_t value) {
    cerf::le::Put16(block.data() + 2u * word, value);
}

void PutAtaString(std::array<uint8_t, 512>& block, std::size_t word, std::size_t bytes,
                  const char* text, bool right_justified) {
    const std::size_t length = std::min(std::strlen(text), bytes);
    const std::size_t pad = right_justified ? bytes - length : 0u;
    for (std::size_t i = 0; i < bytes; ++i) {
        const bool in_text = i >= pad && i < pad + length;
        block[(2u * word + i) ^ 1u] = static_cast<uint8_t>(in_text ? text[i - pad] : ' ');
    }
}

}

void BuildCompactFlashIdentify(std::array<uint8_t, 512>& block, const CompactFlashTaskFile& task_file,
                               uint64_t total_sectors, bool multiple_enabled) {
    block.fill(0);
    const CompactFlashGeometry def = CompactFlashTaskFile::kDefaultGeometry;
    const CompactFlashGeometry cur = task_file.geometry;
    const uint16_t default_cylinders =
        static_cast<uint16_t>(CompactFlashTaskFile::Cylinders(def, total_sectors));
    const uint16_t cylinders = static_cast<uint16_t>(task_file.Cylinders(total_sectors));
    const uint32_t card_sectors = static_cast<uint32_t>(std::min<uint64_t>(total_sectors, 0xFFFFFFFFu));
    const uint32_t current_capacity = static_cast<uint32_t>(cylinders) * cur.heads * cur.sectors;
    const uint32_t lba_sectors = static_cast<uint32_t>(std::min<uint64_t>(total_sectors, kLba28Limit));

    PutWord(block, kWordGeneralConfig, kGeneralConfiguration);
    PutWord(block, kWordCylinders, default_cylinders);
    PutWord(block, kWordHeads, static_cast<uint16_t>(def.heads));
    PutWord(block, kWordSectors, static_cast<uint16_t>(def.sectors));
    PutWord(block, kWordCardSectorsMsw, static_cast<uint16_t>(card_sectors >> 16));
    PutWord(block, kWordCardSectorsLsw, static_cast<uint16_t>(card_sectors));
    PutAtaString(block, kWordSerial, kSerialBytes, "CERF00000001", true);
    PutWord(block, kWordEccCount, kEccByteCount);
    PutAtaString(block, kWordFirmware, kFirmwareBytes, "1.0", false);
    PutAtaString(block, kWordModel, kModelBytes, "CERF CompactFlash", false);
    PutWord(block, kWordMultipleCount, kMultipleSectorCount);
    PutWord(block, kWordCapabilities, kCapabilitiesLba);
    PutWord(block, kWordPioTiming, kPioTimingMode2);
    PutWord(block, kWordValidity, kCurrentChsValid);
    PutWord(block, kWordCurCylinders, cylinders);
    PutWord(block, kWordCurHeads, static_cast<uint16_t>(cur.heads));
    PutWord(block, kWordCurSectors, static_cast<uint16_t>(cur.sectors));
    PutWord(block, kWordCurCapacityLsw, static_cast<uint16_t>(current_capacity));
    PutWord(block, kWordCurCapacityMsw, static_cast<uint16_t>(current_capacity >> 16));
    PutWord(block, kWordMultipleSet,
            static_cast<uint16_t>(kMultipleSettingValid | (multiple_enabled ? kMultipleBlockSectors : 0u)));
    PutWord(block, kWordLbaSectorsLsw, static_cast<uint16_t>(lba_sectors));
    PutWord(block, kWordLbaSectorsMsw, static_cast<uint16_t>(lba_sectors >> 16));
    PutWord(block, kWordCommandSetsSupported, kCommandSetsSupported);
    PutWord(block, kWordCommandSetsSupported2, kCommandSetsSupported2);
    PutWord(block, kWordCommandSetsSupported3, kCommandSetsValid);
    PutWord(block, kWordCommandSetsEnabled, kCommandSetsSupported);
    PutWord(block, kWordCommandSetsEnabled2, kCommandSetsCfa);
    PutWord(block, kWordCommandSetsEnabled3, kCommandSetsValid);
}

}
