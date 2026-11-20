#pragma once

#include <cstdint>

#include "../arm/arm_coproc_provider.h"
#include <cerfengine/arm/arm_mmu_state.h>
#include <cerfengine/calling_convention.h>

namespace cerfcore {

class XscaleCoprocProviderBase : public ArmCoprocProvider {
public:
    using ArmCoprocProvider::ArmCoprocProvider;

protected:
    cerfengine::ArmCoprocDecision RegisterTransfer(const cerfengine::ArmCoprocAccess& a) override;
    cerfengine::ArmCoprocDecision DataTransfer(const cerfengine::ArmCoprocAccess& a) override;
    cerfengine::ArmCoprocDecision DataOperation(const cerfengine::ArmCoprocAccess& a) override;

    virtual cerfengine::ArmCoprocDecision PwrmodeWrite() = 0;
    virtual cerfengine::ArmCoprocDecision UnhandledCoprocessor(
        const cerfengine::ArmCoprocAccess& a) = 0;
    virtual cerfengine::ArmCoprocDecision ClkcfgTransfer(const cerfengine::ArmCoprocAccess& a);

    virtual bool LockChecksAccess() const { return true; }

    cerfengine::ArmCoprocDecision PwrmodeRunIdleSleep();
    cerfengine::ArmCoprocDecision DataCacheLockRegister();
    cerfengine::ArmCoprocDecision LockByMva(cerfengine::ArmCoprocWriteAbort lock);

    uint32_t LockAbort(uint32_t mva, cerfengine::ArmMmuAccess access, bool tlb, bool check_target);
    uint32_t MmuControlWord();
    void     RequireCacheEnabled(uint32_t control_bit, const char* function);
    cerfengine::ArmCoprocDecision CacheUnlock(cerfengine::ArmCoprocWrite unlock);

    static constexpr uint32_t kSctlrDataCache        = 1u << 2;
    static constexpr uint32_t kSctlrInstructionCache = 1u << 12;

    static void CERFENGINE_FASTCALL EnterDeepSleepHook(void* self);
    static uint32_t CERFENGINE_FASTCALL LockInstructionLine(void* self, uint32_t mva);
    static void CERFENGINE_FASTCALL UnlockInstructionCache(void* self, uint32_t value);
    static void CERFENGINE_FASTCALL UnlockDataCache(void* self, uint32_t value);

private:
    cerfengine::ArmCoprocDecision CacheLockdown(const cerfengine::ArmCoprocAccess& a);
    cerfengine::ArmCoprocDecision TlbLockdown(const cerfengine::ArmCoprocAccess& a);

    static uint32_t CERFENGINE_FASTCALL LockInstructionTlb(void* self, uint32_t mva);
    static uint32_t CERFENGINE_FASTCALL LockDataTlb(void* self, uint32_t mva);
    static void CERFENGINE_FASTCALL UnlockTlb(void* self, uint32_t value);
};

}
