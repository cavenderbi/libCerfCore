#include "../../socs/hd6417096/hd6417096_sci_port_nets.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"
#include "casio_cassiopeia_a10_serial_connector.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10SciPortNets : public Hd6417096SciPortNets {
public:
    using Hd6417096SciPortNets::Hd6417096SciPortNets;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { connector_ = &emu_.Get<CasioCassiopeiaA10SerialConnector>(); }

    bool Bit6High() const override { return !connector_->Cts(); }

private:
    CasioCassiopeiaA10SerialConnector* connector_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10SciPortNets, Hd6417096SciPortNets);

}
