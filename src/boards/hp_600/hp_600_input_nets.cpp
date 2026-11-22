#include "hp_600_input_nets.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/hibernation.h>
#include "../sh77xx_port_key_matrix.h"

namespace cerfcore {

namespace {

constexpr uint32_t NetBit(Hp600InputNet net) { return 1u << static_cast<uint32_t>(net); }

constexpr Hp600InputNet kButtonNets[] = {
    Hp600InputNet::kPlayButtonUp,
    Hp600InputNet::kRewindButtonUp,
    Hp600InputNet::kRecordButtonUp,
};

constexpr uint32_t ButtonNetBits() {
    uint32_t bits = 0u;
    for (Hp600InputNet net : kButtonNets) bits |= NetBit(net);
    return bits;
}

constexpr uint32_t kButtonNetBits = ButtonNetBits();

}

void Hp600InputNets::OnReady() {
    intc_   = &emu_.Get<Sh77xxIntc>();
    matrix_ = &emu_.Get<Sh77xxPortKeyMatrix>();
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { ReleaseButtons(); });
}

void Hp600InputNets::Drive(Hp600InputNet net, bool high) {
    if ((NetBit(net) & kButtonNetBits) == 0u) {
        Apply(net, high);
        return;
    }
    std::lock_guard<std::mutex> lk(buttons_mtx_);
    const uint32_t              levels   = Apply(net, high);
    const bool                  any_down = (levels & kButtonNetBits) != kButtonNetBits;
    matrix_->SetColumnPull(any_down ? ButtonSummaryColumns() : uint16_t{0u});
}

bool Hp600InputNets::Wired(Hp600InputNet net) const {
    return Landings()[static_cast<uint32_t>(net)].kind != Landing::Kind::kNone;
}

uint32_t Hp600InputNets::Apply(Hp600InputNet net, bool high) {
    const uint32_t index  = static_cast<uint32_t>(net);
    const uint32_t bit    = NetBit(net);
    const uint32_t before = high ? high_.fetch_or(bit, std::memory_order_acq_rel)
                                 : high_.fetch_and(~bit, std::memory_order_acq_rel);
    const Landing& landing = Landings()[index];
    if (landing.kind == Landing::Kind::kIntcPin) intc_->SetPinLevel(landing.intc, high);
    return high ? before | bit : before & ~bit;
}

void Hp600InputNets::ReleaseButtons() {
    for (Hp600InputNet net : kButtonNets) Drive(net, true);
}

uint8_t Hp600InputNets::WiredInputs(uint32_t port) const {
    uint8_t wired = UndrivenInputs(port);
    for (const Landing& landing : Landings()) {
        if (landing.kind != Landing::Kind::kNone && landing.port == port) wired = static_cast<uint8_t>(wired | landing.mask);
    }
    return wired;
}

uint8_t Hp600InputNets::NetLevels(uint32_t port, uint8_t nets) const {
    const uint32_t                 high     = high_.load(std::memory_order_acquire);
    const std::span<const Landing, kNetCount> landings = Landings();
    for (uint32_t index = 0; index < landings.size(); ++index) {
        const Landing& landing = landings[index];
        if (landing.kind != Landing::Kind::kPortPin || landing.port != port) continue;
        if (((high >> index) & 1u) != 0u) nets = static_cast<uint8_t>(nets | landing.mask);
        else nets = static_cast<uint8_t>(nets & ~landing.mask);
    }
    return nets;
}

}
