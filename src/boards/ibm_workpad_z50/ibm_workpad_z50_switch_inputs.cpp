#include <cerfcore/core/service.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/vr41xx/vr41xx_giu.h"
#include "ibm_workpad_z50_family_id.h"

namespace cerfcore {

namespace {

constexpr int kRecordButtonGpio       = 1;
constexpr int kNotificationButtonGpio = 2;
constexpr int kCoverSwitchGpio        = 11;

class IbmWorkpadZ50SwitchInputs : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
    }

    void OnReady() override {
        auto& giu = emu_.Get<Vr41xxGiu>();
        giu.SetPinLevel(kRecordButtonGpio, true);
        giu.SetPinLevel(kNotificationButtonGpio, true);
        giu.SetPinLevel(kCoverSwitchGpio, true);
    }
};

}

REGISTER_SERVICE(IbmWorkpadZ50SwitchInputs);

}
