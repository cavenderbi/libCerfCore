#include "../../peripherals/ite_it8368/ite_it8368_bus_window.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "sharp_mobilon_hc_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x10400000u;
constexpr uint32_t kSize = 0x22u;

class SharpMobilonHcSocketController : public IteIt8368BusWindow {
public:
    using IteIt8368BusWindow::IteIt8368BusWindow;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SharpMobilonHc;
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

protected:
    bool LanesCrossed() const override { return true; }
};

}

REGISTER_SERVICE(SharpMobilonHcSocketController);

}
