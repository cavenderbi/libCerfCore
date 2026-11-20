#include <cerfcore/host/device_buttons.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_on_key.h"

#include <vector>

namespace cerfcore {

namespace {

constexpr char kOnKeyId[]   = "on_key";
constexpr char kOnKeyName[] = "ON";

class CasioCassiopeiaA10DeviceButtons : public DeviceButtons {
public:
    using DeviceButtons::DeviceButtons;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    std::vector<ButtonSpec> BoardButtons() override {
        return {ButtonSpec{kOnKeyId, kOnKeyName, [this] { emu_.Get<CasioCassiopeiaA10OnKey>().Press(); }}};
    }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10DeviceButtons, DeviceButtons);

}
