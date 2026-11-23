#include "sa1110_sdram_geometry.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/board_database.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../core/board_parts.h"
#include "../../core/ram_generic_part_id.h"
#include "sa1110_id.h"

namespace cerfcore {

REGISTER_SERVICE(Sa1110SdramGeometry);

const DbPart* Sa1110SdramGeometry::RamPart() const { return emu_.Get<BoardParts>().Find(GenericPartId::Ram); }

bool Sa1110SdramGeometry::ShouldRegister() {
    if (emu_.Get<BoardContext>().GetSocId() != SocId::Sa1110) return false;
    const DbPart* ram = RamPart();
    return ram && ram->row_bits;
}

void Sa1110SdramGeometry::OnReady() {
    const DbPart& ram = *RamPart();
    row_bits_    = *ram.row_bits;
    column_bits_ = *ram.column_bits;
    bank_bits_   = *ram.bank_bits;
}

}
