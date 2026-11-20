#include "sh77xx_nmi_input.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "sh77xx_cpg.h"
#include "sh77xx_frqcr.h"

#include <algorithm>

namespace cerfcore {

namespace {

constexpr uint64_t kSettleCycles = 20u;

}

REGISTER_SERVICE(Sh77xxNmiInput);

bool Sh77xxNmiInput::ShouldRegister() { return emu_.Get<BoardContext>().GetCpuArch() == CpuArch::kSh; }

void Sh77xxNmiInput::OnReady() {
    clock_        = &emu_.Get<GuestCycleClock>();
    reset_        = &emu_.Get<GuestCpuReset>();
    cpg_          = &emu_.Get<Sh77xxCpg>();
    frqcr_        = emu_.TryGet<Sh77xxFrqcr>();
    window_event_ = clock_->Add([this] { EndWindowStep(); });
}

void Sh77xxNmiInput::SetLevel(bool high) {
    std::lock_guard<std::mutex> lk(mtx_);
    const bool edge = level_known_ && high != high_ && high == nmie_ && !reset_->Delivering();
    level_known_    = true;
    high_           = high;
    if (!edge || window_ == Window::kUndetected) return;
    if (window_ == Window::kAmbiguous) {
        emu_.Get<Fatal>().Die("Sh77xxNmiInput: NMI edge between 20 CPU clock cycles and 20 peripheral clock cycles "
                              "after an NMIE change");
    }
    pending_ = true;
}

bool Sh77xxNmiInput::High() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!level_known_) {
        emu_.Get<Fatal>().Die("Sh77xxNmiInput: the NMI pin level is needed, and no board signal drives the pin");
    }
    return high_;
}

bool Sh77xxNmiInput::Nmie() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return nmie_;
}

void Sh77xxNmiInput::WriteNmie(bool nmie) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (nmie == nmie_) return;
    if (frqcr_ == nullptr) {
        emu_.Get<Fatal>().Die("Sh77xxNmiInput: an NMIE change needs the peripheral clock rate, and this SoC has no "
                              "FRQCR model");
    }
    nmie_                                  = nmie;
    const GuestCycleClock::Rate cpu        = clock_->ClockRate();
    const GuestCycleClock::Rate peripheral = cpg_->PeripheralRate(frqcr_->Value());
    const uint64_t              now        = clock_->Cycles();
    const uint64_t              cpu_cycles = kSettleCycles * cpu.num * peripheral.den;
    const uint64_t              per_tick   = cpu.den * peripheral.num;
    const uint64_t              slow       = (cpu_cycles + per_tick - 1u) / per_tick;
    undetected_end_                        = now + std::min(kSettleCycles, slow);
    ambiguous_end_                         = now + std::max(kSettleCycles, slow);
    window_                                = Window::kUndetected;
    ArmWindowLocked();
}

bool Sh77xxNmiInput::Pending() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return pending_;
}

void Sh77xxNmiInput::Accept() {
    std::lock_guard<std::mutex> lk(mtx_);
    pending_ = false;
}

void Sh77xxNmiInput::Reset() {
    std::lock_guard<std::mutex> lk(mtx_);
    nmie_    = false;
    pending_ = false;
    window_  = Window::kNone;
    ArmWindowLocked();
}

void Sh77xxNmiInput::SaveState(StateWriter& w) const {
    std::lock_guard<std::mutex> lk(mtx_);
    w.Write("nmi_level_known", level_known_);
    w.Write("nmi_high", high_);
    w.Write("icr0_nmie", nmie_);
    w.Write("nmi_pending", pending_);
    w.Write("nmie_window", static_cast<uint8_t>(window_));
    w.Write("nmie_undetected_end", undetected_end_);
    w.Write("nmie_ambiguous_end", ambiguous_end_);
}

void Sh77xxNmiInput::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(mtx_);
    uint8_t                     window = 0u;
    r.Read("nmi_level_known", level_known_);
    r.Read("nmi_high", high_);
    r.Read("icr0_nmie", nmie_);
    r.Read("nmi_pending", pending_);
    r.Read("nmie_window", window);
    r.Read("nmie_undetected_end", undetected_end_);
    r.Read("nmie_ambiguous_end", ambiguous_end_);
    window_ = static_cast<Window>(window);
}

void Sh77xxNmiInput::PostRestore() {
    std::lock_guard<std::mutex> lk(mtx_);
    ArmWindowLocked();
}

void Sh77xxNmiInput::ArmWindowLocked() {
    if (window_ == Window::kNone) {
        clock_->Disarm(window_event_);
        return;
    }
    clock_->Arm(window_event_, window_ == Window::kUndetected ? undetected_end_ : ambiguous_end_);
}

void Sh77xxNmiInput::EndWindowStep() {
    std::lock_guard<std::mutex> lk(mtx_);
    const bool ambiguous = window_ == Window::kUndetected && ambiguous_end_ > undetected_end_;
    window_              = ambiguous ? Window::kAmbiguous : Window::kNone;
    ArmWindowLocked();
}

}
