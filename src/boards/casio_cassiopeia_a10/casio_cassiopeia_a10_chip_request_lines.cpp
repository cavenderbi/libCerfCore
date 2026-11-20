#include "casio_cassiopeia_a10_request_lines.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_chip_intc.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

namespace cerfcore {

namespace {

class CasioCassiopeiaA10ChipRequestLines : public CasioCassiopeiaA10RequestLines {
public:
    using CasioCassiopeiaA10RequestLines::CasioCassiopeiaA10RequestLines;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               !CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { chip_intc_ = &emu_.Get<CasioCassiopeiaA10ChipIntc>(); }

    void Drive(CasioCassiopeiaA10Request request, bool asserted) override {
        switch (request) {
        case CasioCassiopeiaA10Request::kKeyMatrix:
            chip_intc_->SetInputLevel(CasioCassiopeiaA10ChipInput::kKeyMatrix, asserted);
            return;
        case CasioCassiopeiaA10Request::kPen:
            chip_intc_->SetInputLevel(CasioCassiopeiaA10ChipInput::kPen, asserted);
            return;
        case CasioCassiopeiaA10Request::kOutsideKey:
            chip_intc_->SetInputLevel(CasioCassiopeiaA10ChipInput::kOutsideKey, asserted);
            return;
        case CasioCassiopeiaA10Request::kAudioSwitched:
            chip_intc_->SetInputLevel(CasioCassiopeiaA10ChipInput::kAudioSwitched, asserted);
            return;
        case CasioCassiopeiaA10Request::kAudioEnded:
            chip_intc_->SetInputLevel(CasioCassiopeiaA10ChipInput::kAudioEnded, asserted);
            return;
        }
    }

private:
    CasioCassiopeiaA10ChipIntc* chip_intc_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10ChipRequestLines, CasioCassiopeiaA10RequestLines);

}
