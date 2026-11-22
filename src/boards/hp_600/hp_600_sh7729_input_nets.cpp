#include "hp_600_input_nets.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7729/sh7729_id.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "hp_600_family_id.h"

namespace cerfcore {

namespace {

class Hp600Sh7729InputNets : public Hp600InputNets {
public:
    using Hp600InputNets::Hp600InputNets;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7729;
    }

protected:
    std::span<const Landing, kNetCount> Landings() const override { return kLandings; }
    uint8_t UndrivenInputs(uint32_t port) const override { return port == kSh77xxPortSc ? kPortScIrq5Pin : uint8_t{0u}; }
    uint16_t ButtonSummaryColumns() const override { return kPortC7SummaryColumn; }

private:
    static constexpr uint8_t  kPortScIrq5Pin       = 0x80u;
    static constexpr uint16_t kPortC7SummaryColumn = uint16_t{1u << kHp600PortC7Column};

    static constexpr Landing kLandings[] = {
        {Landing::Kind::kPortPin, Sh77xxIntcPin::kIrq0, kSh77xxPortJ, 0x10u},
        {Landing::Kind::kPortPin, Sh77xxIntcPin::kIrq0, kSh77xxPortD, 0x10u},
        {},
        {Landing::Kind::kPortPin, Sh77xxIntcPin::kIrq0, kSh77xxPortG, 0x08u},
        {Landing::Kind::kPortPin, Sh77xxIntcPin::kIrq0, kSh77xxPortG, 0x10u},
        {Landing::Kind::kPortPin, Sh77xxIntcPin::kIrq0, kSh77xxPortG, 0x20u},
    };
};

}

REGISTER_SERVICE_AS(Hp600Sh7729InputNets, Hp600InputNets);

}
