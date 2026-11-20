#include "casio_cassiopeia_a10_request_lines.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"
#include "casio_cassiopeia_a10_pin_registers.h"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace cerfcore {

namespace {

constexpr uint32_t kRequestPins[] = {
    kCasioCassiopeiaA10Pin058, kCasioCassiopeiaA10Pin056, kCasioCassiopeiaA10Pin054,
    kCasioCassiopeiaA10Pin064, kCasioCassiopeiaA10Pin066,
};
static_assert(std::size(kRequestPins) == static_cast<size_t>(CasioCassiopeiaA10Request::kAudioEnded) + 1u);

class CasioCassiopeiaA10PinRequestLines : public CasioCassiopeiaA10RequestLines {
public:
    using CasioCassiopeiaA10RequestLines::CasioCassiopeiaA10RequestLines;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override { pins_ = &emu_.Get<CasioCassiopeiaA10PinRegisters>(); }

    void Drive(CasioCassiopeiaA10Request request, bool asserted) override {
        pins_->SetLine(kRequestPins[static_cast<size_t>(request)], !asserted);
    }

private:
    CasioCassiopeiaA10PinRegisters* pins_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10PinRequestLines, CasioCassiopeiaA10RequestLines);

}
