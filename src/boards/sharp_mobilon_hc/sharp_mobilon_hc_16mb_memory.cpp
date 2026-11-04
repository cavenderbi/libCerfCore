#include "sharp_mobilon_hc_memory.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "sharp_mobilon_hc4500_id.h"
#include "sharp_mobilon_hc4600_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

class SharpMobilonHc16mbMemory : public SharpMobilonHcMemory {
public:
    using SharpMobilonHcMemory::SharpMobilonHcMemory;

    bool ShouldRegister() override {
        const std::string_view id = emu_.Get<BoardContext>().GetBoardId();
        return id == BoardId::SharpMobilonHc4500 || id == BoardId::SharpMobilonHc4600;
    }

    uint32_t MemConfig0Banks() const override { return 0x00013030u; }
    uint32_t Bank0Bytes() const override { return 0x01000000u; }
    uint32_t Bank1Bytes() const override { return 0u; }
    bool     Cs1RamTag() const override { return false; }
};

}

REGISTER_SERVICE_AS(SharpMobilonHc16mbMemory, SharpMobilonHcMemory);

}
