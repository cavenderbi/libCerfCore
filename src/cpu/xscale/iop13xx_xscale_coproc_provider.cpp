#include "xscale_coproc_provider_base.h"

#include "../arm/arm_page_walker.h"
#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/iop13xx/iop13xx_cp6.h"
#include "../../socs/iop13xx/iop13xx_id.h"

namespace cerfcore {

namespace {

using cerfengine::ArmCoprocAccess;
using cerfengine::ArmCoprocDecision;
using A = cerfengine::ArmCoprocAction;
using C = cerfengine::ArmCoprocCase;

class Iop13xxXscaleCoprocProvider final : public XscaleCoprocProviderBase {
public:
    using XscaleCoprocProviderBase::XscaleCoprocProviderBase;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == SocId::Iop13xx;
    }

protected:
    ArmCoprocDecision RegisterTransfer(const ArmCoprocAccess& a) override {
        if (a.cp_num == 15u && !a.read && a.crn == 7u && a.opc1 == 1u) {
            const bool invalidate_mva   = a.crm == 7u && a.opc2 == 1u;
            const bool clean            = a.crm == 11u && (a.opc2 == 1u || a.opc2 == 2u);
            const bool clean_invalidate = a.crm == 15u && a.opc2 == 2u;
            if (invalidate_mva || clean || clean_invalidate) return {.action = A::kNoOp};
        }
        if (a.cp_num == 15u && a.crn == 9u && (a.crm == 5u || a.crm == 6u)) return CacheLockdownL2(a);
        if (a.cp_num == 6u) return emu_.Get<Iop13xxCp6>().Decide(a);
        if (a.cp_num == 7u) return {.action = A::kUnimplemented};
        return XscaleCoprocProviderBase::RegisterTransfer(a);
    }

    /* Third Generation Intel XScale Microarchitecture Developer's Manual Table
       66: M "0 = ACTIVE mode", "All other values are defined by the ASSP"; the
       Linux XSC3 model cpu_xsc3_do_idle writes M = 1 to "go to idle". */
    ArmCoprocDecision PwrmodeWrite() override {
        return {.action     = A::kWriteValueSwitch,
                .case_count = 2u,
                .cases      = {{1u, C::kWaitForInterrupt}, {0u, C::kContinue}}};
    }

    ArmCoprocDecision UnhandledCoprocessor(const ArmCoprocAccess& a) override {
        if (a.cp_num == 6u || a.cp_num == 7u || a.cp_num == 14u) {
            return {.action = A::kUnimplemented};
        }
        return {.action = A::kUndefined};
    }

    bool LockChecksAccess() const override { return false; }

private:
    ArmCoprocDecision CacheLockdownL2(const ArmCoprocAccess& a) {
        if (a.opc1 == 0u && a.crm == 6u && a.opc2 == 0u) return DataCacheLockRegister();
        if (a.read) return {.action = A::kUnimplemented};
        if (a.opc1 == 0u && a.crm == 5u && a.opc2 == 0u) {
            return LockByMva(&XscaleCoprocProviderBase::LockInstructionLine);
        }
        if (a.opc1 == 0u && a.opc2 == 1u) {
            return CacheUnlock(a.crm == 5u ? &XscaleCoprocProviderBase::UnlockInstructionCache
                                           : &XscaleCoprocProviderBase::UnlockDataCache);
        }
        if (a.opc1 == 1u && a.crm == 5u && a.opc2 == 1u) return CacheUnlock(&UnlockL2Cache);
        if (a.opc1 == 1u && a.crm == 5u && (a.opc2 == 0u || a.opc2 == 2u)) {
            return LockByMva(&Iop13xxXscaleCoprocProvider::LockL2Line);
        }
        return {.action = A::kUnimplemented};
    }

    static constexpr uint32_t kSctlrL2Cache = 1u << 26;

    static void CERFENGINE_FASTCALL UnlockL2Cache(void* self, uint32_t) {
        static_cast<Iop13xxXscaleCoprocProvider*>(self)->RequireCacheEnabled(kSctlrL2Cache,
                                                                             "L2 cache unlock");
    }

    static uint32_t CERFENGINE_FASTCALL LockL2Line(void* self, uint32_t mva) {
        auto* provider = static_cast<Iop13xxXscaleCoprocProvider*>(self);
        provider->RequireCacheEnabled(kSctlrL2Cache, "L2 cache lock");
        const bool translated = provider->emu_.Get<ArmPageWalker>().TranslatesForLock(
            mva, cerfengine::ArmMmuAccess::kRead, false, false, provider->MmuControlWord());
        return translated ? 0u : 1u;
    }
};

}

REGISTER_SERVICE_AS(Iop13xxXscaleCoprocProvider, ArmCoprocProvider);

}
