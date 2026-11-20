#include "casio_cassiopeia_a10_microphone.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10AbsentMicrophone : public CasioCassiopeiaA10Microphone {
public:
    using CasioCassiopeiaA10Microphone::CasioCassiopeiaA10Microphone;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               !CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void Open(uint32_t) override {}

    void Fill(uint8_t* slots, uint32_t count, uint32_t slot_bytes) override { Silence(slots, count, slot_bytes); }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10AbsentMicrophone, CasioCassiopeiaA10Microphone);

}
