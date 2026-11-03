#include "philips_velo_1_dram_card_id_eeprom.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>
#include "philips_velo_1_dram_card_profile.h"
#include "philips_velo_1_family_id.h"

namespace cerfcore {

namespace {

constexpr uint8_t kDeviceAddress = 0x50u;
constexpr uint8_t kSignature     = 0x99u;
constexpr uint8_t kTupleCount    = 1u;
constexpr uint8_t kTupleType     = 2u;
constexpr uint8_t kDevicePresent = 0x01u;
constexpr uint8_t kRefreshByte   = 127u;

}

REGISTER_SERVICE(PhilipsVelo1DramCardIdEeprom);

bool PhilipsVelo1DramCardIdEeprom::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetBoardFamilyId() == BoardFamilyId::PhilipsVelo1 &&
           FindPhilipsVelo1DramCard(bd->GetBoardId()) != nullptr;
}

void PhilipsVelo1DramCardIdEeprom::OnReady() {
    const PhilipsVelo1DramCardProfile& card =
        *FindPhilipsVelo1DramCard(emu_.Get<BoardContext>().GetBoardId());
    id_rom_.fill(0);
    id_rom_[16]  = kSignature;
    id_rom_[59]  = kTupleCount;
    id_rom_[64]  = kTupleType;
    id_rom_[67]  = static_cast<uint8_t>(card.megabytes - 1u);
    id_rom_[96]  = card.row_bits;
    id_rom_[97]  = card.column_bits;
    id_rom_[98]  = kDevicePresent;
    id_rom_[101] = kRefreshByte;
}

bool PhilipsVelo1DramCardIdEeprom::MatchesAddress(uint8_t slave_addr) const {
    return slave_addr == kDeviceAddress;
}

void PhilipsVelo1DramCardIdEeprom::TxnStart(uint8_t) { word_addr_pending_ = true; }

void PhilipsVelo1DramCardIdEeprom::TxnWriteByte(uint8_t, uint8_t byte) {
    if (!word_addr_pending_) {
        emu_.Get<Fatal>().Die("Philips Velo 1 DRAM card: ID EEPROM write of 0x%02X to word 0x%02X",
                              byte, word_addr_);
    }
    word_addr_         = byte;
    word_addr_pending_ = false;
}

uint8_t PhilipsVelo1DramCardIdEeprom::TxnReadByte(uint8_t) {
    word_addr_pending_ = false;
    const uint8_t v = id_rom_[word_addr_];
    word_addr_ = static_cast<uint8_t>(word_addr_ + 1u);
    return v;
}

void PhilipsVelo1DramCardIdEeprom::SaveState(StateWriter& w) {
    w.Write("word_addr", word_addr_);
    w.Write("word_addr_pending", static_cast<uint8_t>(word_addr_pending_));
}

void PhilipsVelo1DramCardIdEeprom::RestoreState(StateReader& r) {
    uint8_t v = 0;
    r.Read("word_addr", word_addr_);
    r.Read("word_addr_pending", v);
    word_addr_pending_ = v != 0u;
}

}
