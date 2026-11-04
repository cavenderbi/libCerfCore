#include "../../peripherals/ite_it8181/ite_it8181_config.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "ibm_workpad_z50_family_id.h"

namespace cerfcore {

namespace {

class IbmWorkpadZ50It8181Config : public IteIt8181Config {
public:
    using IteIt8181Config::IteIt8181Config;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
    }

    uint32_t HostWindowBase() const override { return 0x0A000000u; }

    uint8_t Revision() const override { return 0x04u; }
};

}

REGISTER_SERVICE_AS(IbmWorkpadZ50It8181Config, IteIt8181Config);

}
