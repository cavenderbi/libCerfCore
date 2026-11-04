#include "../../socs/vr4121/vr4121_bcu_board.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/vr41xx/vr41xx_bcu_regs.h"
#include "../../socs/vr4121/vr4121_bcu_regs.h"
#include <cerfcore/boards/board_context.h>
#include "ibm_workpad_z50_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBank0ChipBytes = 0x01000000u;

class IbmWorkpadZ50Vr4121BcuBoard : public Vr4121BcuBoard {
public:
    using Vr4121BcuBoard::Vr4121BcuBoard;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
    }

    bool Sdram() const override { return false; }

    std::optional<Vr4121DramWiring> DramWiring() const override {
        return Vr4121DramWiring{ true, { kBank0ChipBytes, 0u, 0u, 0u } };
    }

    std::vector<Vr41xxBcuBootWrite> KernelEntryWrites() const override {
        return {
            { vr41xx_bcu::kOffCnt1, vr4121_bcu::kCnt1Rom64 | vr4121_bcu::kCnt1Dram64 },
        };
    }

    std::optional<Vr4121Revision> Revision() const override { return Vr4121Revision{ 0u, 0u }; }
};

}

REGISTER_SERVICE_AS(IbmWorkpadZ50Vr4121BcuBoard, Vr4121BcuBoard);

}
