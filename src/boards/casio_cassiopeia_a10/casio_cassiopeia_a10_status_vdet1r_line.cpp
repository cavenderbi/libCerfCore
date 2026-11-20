#include "casio_cassiopeia_a10_vdet1r_line.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_battery.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint16_t kStatusBit9 = 0x0200u;

class CasioCassiopeiaA10StatusVdet1rLine : public CasioCassiopeiaA10Vdet1rLine {
public:
    using CasioCassiopeiaA10Vdet1rLine::CasioCassiopeiaA10Vdet1rLine;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { battery_ = &emu_.Get<CasioCassiopeiaA10Battery>(); }

    uint16_t StatusBits() const override {
        return battery_->Tripped(CasioCassiopeiaA10Vdet::kVdet1) ? kStatusBit9 : 0u;
    }
    uint8_t PortPins() const override { return 0u; }
    uint8_t PortLevels() const override { return 0u; }

private:
    CasioCassiopeiaA10Battery* battery_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10StatusVdet1rLine, CasioCassiopeiaA10Vdet1rLine);

}
