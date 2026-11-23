#include "vrc4172_pcs.h"

#include "../peripheral_dispatcher.h"
#include "../../core/board_parts.h"
#include "nec_vrc4172_part_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x15001090u;

constexpr uint32_t kSell = 0u;
constexpr uint32_t kSelh = 1u;
constexpr uint32_t kMskl = 2u;
constexpr uint32_t kMskh = 3u;

constexpr uint16_t kWritable[] = {0xFFFEu, 0x01FFu, 0xFFFFu, 0x01FFu};

constexpr uint16_t kCsEnable    = 0x0001u;
constexpr uint32_t kDecodedBits = 0x01FFFFFEu;

}

REGISTER_SERVICE(Vrc4172Pcs);

bool Vrc4172Pcs::ShouldRegister() {
    return emu_.Get<BoardParts>().Has(PartId::NecVrc4172);
}

void Vrc4172Pcs::OnReady() {
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { ApplyReset(); });
    emu_.Get<PeripheralDispatcher>().Register(this);
}

uint32_t Vrc4172Pcs::MmioBase() const { return kBase; }
uint32_t Vrc4172Pcs::MmioSize() const { return kRegCount * 2u; }

uint16_t Vrc4172Pcs::ReadHalf(uint32_t addr) {
    return regs_[(addr - kBase) / 2u];
}

void Vrc4172Pcs::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t idx = (addr - kBase) / 2u;
    regs_[idx] = static_cast<uint16_t>(value & kWritable[idx % kRegsPerChannel]);
}

bool Vrc4172Pcs::Selects(uint32_t channel, uint32_t pa) const {
    const uint16_t* r = &regs_[channel * kRegsPerChannel];
    if ((r[kMskl] & kCsEnable) == 0u) return false;
    const uint32_t sel  = (static_cast<uint32_t>(r[kSelh]) << 16) | r[kSell];
    const uint32_t mask = (static_cast<uint32_t>(r[kMskh]) << 16) | (r[kMskl] & ~kCsEnable);
    return ((pa ^ sel) & ~mask & kDecodedBits) == 0u;
}

void Vrc4172Pcs::SaveState(StateWriter& w) {
    w.Write("regs", regs_);
}

void Vrc4172Pcs::RestoreState(StateReader& r) {
    r.Read("regs", regs_);
}

void Vrc4172Pcs::ApplyReset() {
    for (auto& reg : regs_) reg = 0u;
}

}
