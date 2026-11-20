#include "casio_cassiopeia_a10_rts_line.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/hd6417096/hd6417096_scif_port_pins.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

#include <utility>

namespace cerfcore {

namespace {

class CasioCassiopeiaA10ScsptrRtsLine : public CasioCassiopeiaA10RtsLine {
public:
    using CasioCassiopeiaA10RtsLine::CasioCassiopeiaA10RtsLine;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { pins_ = &emu_.Get<Hd6417096ScifPortPins>(); }

    bool Asserted() const override { return !pins_->RtsHigh(); }

    void AddChangeListener(CallbackList<>::Callback listener) override { pins_->AddChangeListener(std::move(listener)); }

private:
    Hd6417096ScifPortPins* pins_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10ScsptrRtsLine, CasioCassiopeiaA10RtsLine);

}
