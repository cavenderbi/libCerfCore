#include "vr41xx_aiu_interrupts.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>
#include "vr41xx_icu.h"

namespace cerfcore {

void Vr41xxAiuInterrupts::Attach() { icu_ = &emu_.Get<Vr41xxIcu>(); }

void Vr41xxAiuInterrupts::Raise(uint16_t bits) {
    if ((bits_ | bits) == bits_) return;
    bits_ = static_cast<uint16_t>(bits_ | bits);
    Publish();
}

uint16_t Vr41xxAiuInterrupts::Clear(uint16_t bits) {
    const uint16_t cleared = static_cast<uint16_t>(bits_ & bits);
    if (cleared == 0u) return 0u;
    bits_ = static_cast<uint16_t>(bits_ & ~cleared);
    Publish();
    return cleared;
}

void Vr41xxAiuInterrupts::Reset() {
    bits_ = 0u;
    Publish();
}

void Vr41xxAiuInterrupts::Publish() { icu_->SetAiuSource(bits_); }

void Vr41xxAiuInterrupts::Save(StateWriter& w) const { w.Write("int", bits_); }

void Vr41xxAiuInterrupts::Restore(StateReader& r) { r.Read("int", bits_); }

}
