#include "../xscale/xscale_coproc_provider_base.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/pxa27x/pxa270_id.h"
#include "../../socs/pxa27x/pxa27x_clock_manager.h"
#include "../../socs/pxa27x/pxa27x_power_manager.h"

namespace cerfcore {

namespace {

using cerfengine::ArmCoprocAccess;
using cerfengine::ArmCoprocDecision;
using A = cerfengine::ArmCoprocAction;
using C = cerfengine::ArmCoprocCase;

uint32_t CparGate(uint32_t cp_num) {
    return cp_num > 13u ? 0u : (1u << cp_num);
}

class Pxa27xCoprocProvider : public XscaleCoprocProviderBase {
public:
    using XscaleCoprocProviderBase::XscaleCoprocProviderBase;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == SocId::Pxa270;
    }

protected:
    ArmCoprocDecision PwrmodeWrite() override {
        return {.action     = A::kWriteValueSwitch,
                .context    = this,
                .case_count = 4u,
                .cases      = {{1u, C::kWaitForInterrupt},
                               {3u, C::kHook, &XscaleCoprocProviderBase::EnterDeepSleepHook},
                               {8u, C::kHook, &Pxa27xCoprocProvider::VoltageChangeHook},
                               {0u, C::kContinue}}};
    }

    ArmCoprocDecision UnhandledCoprocessor(const ArmCoprocAccess& a) override {
        return {.action = A::kUnimplemented, .gate_mask = CparGate(a.cp_num)};
    }

    static void CERFENGINE_FASTCALL VoltageChangeHook(void* self) {
        static_cast<Pxa27xCoprocProvider*>(self)->emu_.Get<Pxa27xPowerManager>().StartVoltageChange();
    }

    ArmCoprocDecision ClkcfgTransfer(const ArmCoprocAccess& a) override {
        Pxa27xClockManager* clocks = &emu_.Get<Pxa27xClockManager>();
        if (a.read) {
            return {.action = A::kReadCallback, .context = clocks,
                    .read = &Pxa27xClockManager::ReadClkcfgHelper};
        }
        return {.action = A::kWriteCallback, .context = clocks,
                .write = &Pxa27xClockManager::WriteClkcfgHelper};
    }
};

}

REGISTER_SERVICE_AS(Pxa27xCoprocProvider, ArmCoprocProvider);

}
