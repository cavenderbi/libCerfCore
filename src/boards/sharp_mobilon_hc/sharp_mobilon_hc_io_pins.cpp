#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/pr31x00/pr31x00_io_pins.h"
#include <cerfcore/boards/board_context.h>
#include "sharp_mobilon_hc_family_id.h"
#include "sharp_mobilon_hc_cmtt.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

namespace {

constexpr uint32_t kCmttReadReady = 0x00010000u;

class SharpMobilonHcIoPins : public Pr31x00IoPins {
public:
    using Pr31x00IoPins::Pr31x00IoPins;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SharpMobilonHc;
    }

    uint32_t IoDin() const override { return 0u; }

    std::optional<uint32_t> MfioDin() const override {
        return emu_.Get<SharpMobilonHcCmtt>().ReadReady() ? kCmttReadReady : 0u;
    }
};

}  /* namespace */

REGISTER_SERVICE_AS(SharpMobilonHcIoPins, Pr31x00IoPins);

}
