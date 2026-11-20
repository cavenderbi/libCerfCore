#include "hd6417096_on_key_input.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>
#include "../reg_window_impl.h"
#include "hd6417096_id.h"

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace cerfcore {

namespace {

using cerf_reg_window_detail::OtherReset;
using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegWindowBase;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

constexpr uint32_t kCauses  = 0u;
constexpr uint32_t kEnables = 1u;

constexpr uint16_t kOnKeyCause  = 0x0002u;
constexpr uint16_t kOnKeyEnable = 0x0002u;
constexpr uint16_t kOnKeyHeld   = 0x0002u;

constexpr uint16_t kGuestClears[] = {0xFFFFu, 0x0001u};

constexpr RegWindowModel kModel = {
    0x1FFFFE00u,
    6u,
    3u,
    false,
    {
        {ReadKind::kStored, WriteKind::kClear, 0xFFFFu, 0x0000u, 0x0000u, OtherReset::kReset},
        {ReadKind::kFatal, WriteKind::kStored, kOnKeyEnable, 0x0000u, static_cast<uint16_t>(~kOnKeyEnable)},
        {ReadKind::kSampled, WriteKind::kFatal},
    },
};

class Hd6417096WakeControl : public RegWindowBase<SocId::Hd6417096, kModel, Hd6417096OnKeyInput> {
public:
    using RegWindowBase::RegWindowBase;

    void SetOnKeyHeld(bool held, bool cpu_running) override {
        held_.store(held ? kOnKeyHeld : 0u, std::memory_order_release);
        if (!held || cpu_running || (StoredReg(kEnables) & kOnKeyEnable) == 0u) return;
        SetStoredReg(kCauses, static_cast<uint16_t>(StoredReg(kCauses) | kOnKeyCause));
    }

    void SaveState(StateWriter& w) override {
        RegWindowBase::SaveState(w);
        w.Write("on_key_held", held_.load(std::memory_order_acquire));
    }

    void RestoreState(StateReader& r) override {
        RegWindowBase::RestoreState(r);
        uint16_t held = 0u;
        r.Read("on_key_held", held);
        held_.store(held, std::memory_order_release);
    }

protected:
    uint16_t SampleRegister(uint32_t, uint32_t) override { return held_.load(std::memory_order_acquire); }

    void BeforeClear(uint32_t, uint16_t value) override {
        if (std::ranges::find(kGuestClears, value) == std::ranges::end(kGuestClears)) {
            emu_.Get<Fatal>().Die("Hd6417096WakeControl: write 0x%04X to the wake causes", value);
        }
    }

private:
    std::atomic<uint16_t> held_{0u};
};

}

REGISTER_SERVICE_AS(Hd6417096WakeControl, Hd6417096OnKeyInput);

}
