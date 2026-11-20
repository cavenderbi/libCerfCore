#include "casio_cassiopeia_a10_rts_line.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

#include <cstdint>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kPort   = 0u;
constexpr uint32_t kRtsPin = 6u;

class CasioCassiopeiaA10PortRtsLine : public CasioCassiopeiaA10RtsLine {
public:
    using CasioCassiopeiaA10RtsLine::CasioCassiopeiaA10RtsLine;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               !CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { ports_ = &emu_.Get<Sh77xxPortData>(); }

    bool Asserted() const override { return ports_->Drive(kPort, kRtsPin) == PinDrive::kLow; }

    void AddChangeListener(CallbackList<>::Callback listener) override { ports_->AddDriveListener(std::move(listener)); }

private:
    Sh77xxPortData* ports_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10PortRtsLine, CasioCassiopeiaA10RtsLine);

}
