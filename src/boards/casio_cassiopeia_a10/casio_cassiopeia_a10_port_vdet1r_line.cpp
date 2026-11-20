#include "casio_cassiopeia_a10_vdet1r_line.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_battery.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint8_t kPin0 = 0x01u;

class CasioCassiopeiaA10PortVdet1rLine : public CasioCassiopeiaA10Vdet1rLine {
public:
    using CasioCassiopeiaA10Vdet1rLine::CasioCassiopeiaA10Vdet1rLine;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               !CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { battery_ = &emu_.Get<CasioCassiopeiaA10Battery>(); }

    uint16_t StatusBits() const override { return 0u; }
    uint8_t  PortPins() const override { return kPin0; }
    uint8_t  PortLevels() const override { return battery_->Tripped(CasioCassiopeiaA10Vdet::kVdet1) ? 0u : kPin0; }

private:
    CasioCassiopeiaA10Battery* battery_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10PortVdet1rLine, CasioCassiopeiaA10Vdet1rLine);

}
