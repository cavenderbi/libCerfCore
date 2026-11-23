#include "hp_jornada_540_eeprom.h"

#include <cerfcore/boards/board_context.h>
#include "../../core/board_parts.h"
#include "hp_jornada_540_eeprom_part_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/device_config.h>
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

constexpr uint8_t  kDeviceAddress    = 0x50u;
constexpr uint16_t kPageBytesUnknown = 0u;
constexpr uint8_t  kBlankCell        = 0xFFu;

}

REGISTER_SERVICE(HpJornada540Eeprom);

bool HpJornada540Eeprom::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540 &&
           emu_.Get<BoardParts>().Has(PartId::HpJornada540Eeprom);
}

void HpJornada540Eeprom::FillCells() { Cells().fill(kBlankCell); }

uint8_t HpJornada540Eeprom::DeviceAddress() const { return kDeviceAddress; }

uint16_t HpJornada540Eeprom::PageBytes() const { return kPageBytesUnknown; }

const char* HpJornada540Eeprom::PartName() const { return "Jornada 540 configuration EEPROM"; }

std::string HpJornada540Eeprom::StoragePath() const {
    const DeviceConfig& cfg = emu_.Get<DeviceConfig>();
    return ResolveDeviceFile(cfg.device_name, cfg.storage_eeprom);
}

}
