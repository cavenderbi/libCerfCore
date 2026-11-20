#include "sh77xx_standby_entry.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/guest_deep_sleep.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(Sh77xxStandbyEntry);

bool Sh77xxStandbyEntry::ShouldRegister() { return emu_.Get<BoardContext>().GetCpuArch() == CpuArch::kSh; }

void Sh77xxStandbyEntry::OnReady() {
    emu_.Get<GuestDeepSleep>().RegisterSleepEntryListener([this] { Enter(); });
}

void Sh77xxStandbyEntry::RegisterListener(std::function<void()> listener) { listeners_.Add(std::move(listener)); }

void Sh77xxStandbyEntry::RegisterWakeCheck(std::function<void()> check) { wake_checks_.Add(std::move(check)); }

void Sh77xxStandbyEntry::Enter() {
    listeners_.Call();
    wake_checks_.Call();
}

}
