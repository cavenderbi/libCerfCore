#include "xscale_coproc_provider_base.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include "../arm/arm_cp15_decisions.h"
#include "../arm/arm_cp15_registers.h"
#include "../arm/arm_page_walker.h"
#include "../../execution/engine_bridge.h"
#include <cerfcore/core/fatal.h>
#include <cerfengine/arm/arm_mmu.h>
#include <cerfengine/arm/arm_mmu_state.h>

namespace cerfcore {

using cerfengine::ArmCoprocAccess;
using cerfengine::ArmCoprocDecision;
using A = cerfengine::ArmCoprocAction;
using C = cerfengine::ArmCoprocCase;

namespace {

constexpr uint32_t kSctlrMmuEnable  = 1u << 0;
constexpr uint32_t kLockAbortStatus = (1u << 10) | 0x4u;

}

ArmCoprocDecision XscaleCoprocProviderBase::RegisterTransfer(const ArmCoprocAccess& a) {
    if (a.cp_num == 15u) {
        if (a.crn == 15u && a.crm == 1u && a.opc2 == 0u && a.opc1 == 0u) {
            return {.action = A::kCoprocessorAccess};
        }
        if (a.crn == 14u) {
            const bool is_dbg = a.opc1 == 0u && a.opc2 == 0u &&
                (a.crm == 0u || a.crm == 3u || a.crm == 4u || a.crm == 8u || a.crm == 9u);
            if (!is_dbg) return {.action = A::kUnimplemented};
            if (a.read) return {.action = A::kConstant, .constant = 0u};
            return {.action = A::kNoOp};
        }
        /* XScale Core Dev Manual Table 7-3 (page 80): register 1 is CRm=0 only;
           Table 7-1 (page 78): CRm "should be programmed to zero ... unless a
           value has been specified in the command". */
        if (a.crn == 1u && a.opc1 == 0u && a.crm != 0u) {
            if (a.crm != 1u || a.opc2 != 0u) return {.action = A::kUnimplemented};
            if (a.read) return {.action = A::kConstant, .constant = 0u};
            return {.action = A::kNoOp};
        }
        if (a.crn == 7u && a.crm == 2u && a.opc2 == 5u && a.opc1 == 0u) {
            return {.action = A::kNoOp};
        }
        if (a.crn == 9u) return CacheLockdown(a);
        if (a.crn == 10u) return TlbLockdown(a);
        return Cp15().Decide(a);
    }
    if (a.cp_num == 14u && a.opc1 == 0u && a.crm == 0u && a.opc2 == 0u) {
        if (a.crn == 7u) {
            if (a.read) return {.action = A::kConstant, .constant = 0u};
            return PwrmodeWrite();
        }
        /* XScale Core Dev Manual Table 7-25: CCLKCFG "is defined by the
           ASSP". */
        if (a.crn == 6u) return ClkcfgTransfer(a);
        if (a.read) return {.action = A::kConstant, .constant = 0u};
        return {.action = A::kNoOp};
    }
    return UnhandledCoprocessor(a);
}

ArmCoprocDecision XscaleCoprocProviderBase::CacheLockdown(const ArmCoprocAccess& a) {
    if (a.opc1 != 0u) return {.action = A::kUnimplemented};
    if (a.crm == 2u && a.opc2 == 0u) return DataCacheLockRegister();
    if (a.read) return {.action = A::kUnimplemented};
    if (a.crm == 1u && a.opc2 == 0u) return LockByMva(&XscaleCoprocProviderBase::LockInstructionLine);
    if (a.crm == 1u && a.opc2 == 1u) return CacheUnlock(&XscaleCoprocProviderBase::UnlockInstructionCache);
    if (a.crm == 2u && a.opc2 == 1u) return CacheUnlock(&XscaleCoprocProviderBase::UnlockDataCache);
    return {.action = A::kUnimplemented};
}

ArmCoprocDecision XscaleCoprocProviderBase::CacheUnlock(cerfengine::ArmCoprocWrite unlock) {
    return {.action = A::kWriteCallback, .context = this, .write = unlock};
}

void XscaleCoprocProviderBase::RequireCacheEnabled(uint32_t control_bit, const char* function) {
    if ((MmuControlWord() & control_bit) == 0u)
        emu_.Get<Fatal>().Die("XScale %s with that cache disabled (control 0x%08X)", function,
                              MmuControlWord());
}

void CERFENGINE_FASTCALL XscaleCoprocProviderBase::UnlockInstructionCache(void* self, uint32_t) {
    static_cast<XscaleCoprocProviderBase*>(self)->RequireCacheEnabled(kSctlrInstructionCache,
                                                                      "instruction cache unlock");
}

void CERFENGINE_FASTCALL XscaleCoprocProviderBase::UnlockDataCache(void* self, uint32_t) {
    static_cast<XscaleCoprocProviderBase*>(self)->RequireCacheEnabled(kSctlrDataCache,
                                                                      "data cache unlock");
}

ArmCoprocDecision XscaleCoprocProviderBase::TlbLockdown(const ArmCoprocAccess& a) {
    if (a.read || a.opc1 != 0u || (a.crm != 4u && a.crm != 8u)) return {.action = A::kUnimplemented};
    if (a.opc2 == 1u) return {.action = A::kWriteCallback, .context = this, .write = &UnlockTlb};
    if (a.opc2 != 0u) return {.action = A::kUnimplemented};
    return LockByMva(a.crm == 4u ? &XscaleCoprocProviderBase::LockInstructionTlb
                                 : &XscaleCoprocProviderBase::LockDataTlb);
}

ArmCoprocDecision XscaleCoprocProviderBase::DataCacheLockRegister() {
    return {.action = A::kRegister, .reg = &emu_.Get<ArmCp15Registers>().File().data_cache_lock};
}

ArmCoprocDecision XscaleCoprocProviderBase::LockByMva(cerfengine::ArmCoprocWriteAbort lock) {
    return {.action = A::kWriteCallbackAbort, .context = this, .write_abort = lock};
}

uint32_t XscaleCoprocProviderBase::MmuControlWord() {
    return emu_.Get<EngineBridge>().Engine().Get<cerfengine::ArmMmu>().State()->effective_control_register.word;
}

uint32_t XscaleCoprocProviderBase::LockAbort(uint32_t mva, cerfengine::ArmMmuAccess access,
                                             bool tlb, bool check_target) {
    const uint32_t control = MmuControlWord();
    if (tlb && (control & kSctlrMmuEnable) == 0u)
        emu_.Get<Fatal>().Die("XScale TLB lock of MVA 0x%08X with the MMU disabled", mva);
    const bool check_access = LockChecksAccess();
    if (emu_.Get<ArmPageWalker>().TranslatesForLock(mva, access, check_access,
                                                     check_access || check_target, control))
        return 0u;
    emu_.Get<ArmCp15Registers>().File().dfsr = kLockAbortStatus;
    return 1u;
}

uint32_t CERFENGINE_FASTCALL XscaleCoprocProviderBase::LockInstructionLine(void* self, uint32_t mva) {
    auto* provider = static_cast<XscaleCoprocProviderBase*>(self);
    provider->RequireCacheEnabled(kSctlrInstructionCache, "instruction cache fetch-and-lock");
    return provider->LockAbort(mva, cerfengine::ArmMmuAccess::kExecute, false, true);
}

uint32_t CERFENGINE_FASTCALL XscaleCoprocProviderBase::LockInstructionTlb(void* self, uint32_t mva) {
    return static_cast<XscaleCoprocProviderBase*>(self)->LockAbort(
        mva, cerfengine::ArmMmuAccess::kExecute, true, false);
}

uint32_t CERFENGINE_FASTCALL XscaleCoprocProviderBase::LockDataTlb(void* self, uint32_t mva) {
    return static_cast<XscaleCoprocProviderBase*>(self)->LockAbort(
        mva, cerfengine::ArmMmuAccess::kRead, true, false);
}

void CERFENGINE_FASTCALL XscaleCoprocProviderBase::UnlockTlb(void* self, uint32_t) {
    auto* provider = static_cast<XscaleCoprocProviderBase*>(self);
    if ((provider->MmuControlWord() & kSctlrMmuEnable) == 0u)
        provider->emu_.Get<Fatal>().Die("XScale TLB unlock with the MMU disabled");
}

ArmCoprocDecision XscaleCoprocProviderBase::ClkcfgTransfer(const ArmCoprocAccess&) {
    return {.action = A::kUnimplemented};
}

ArmCoprocDecision XscaleCoprocProviderBase::DataTransfer(const ArmCoprocAccess& a) {
    return UnhandledCoprocessor(a);
}

ArmCoprocDecision XscaleCoprocProviderBase::DataOperation(const ArmCoprocAccess& a) {
    return UnhandledCoprocessor(a);
}

ArmCoprocDecision XscaleCoprocProviderBase::PwrmodeRunIdleSleep() {
    return {.action     = A::kWriteValueSwitch,
            .context    = this,
            .case_count = 3u,
            .cases      = {{1u, C::kWaitForInterrupt},
                           {3u, C::kHook, &XscaleCoprocProviderBase::EnterDeepSleepHook},
                           {0u, C::kContinue}}};
}

void CERFENGINE_FASTCALL XscaleCoprocProviderBase::EnterDeepSleepHook(void* self) {
    static_cast<XscaleCoprocProviderBase*>(self)->emu_.Get<GuestDeepSleep>().Enter();
}

}
