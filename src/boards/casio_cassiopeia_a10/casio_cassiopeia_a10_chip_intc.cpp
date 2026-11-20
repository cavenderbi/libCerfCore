#include "casio_cassiopeia_a10_chip_intc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "casio_cassiopeia_a10_family_id.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10ChipIntc);

namespace {

constexpr uint32_t kLatchMode = 0x10000028u;
constexpr uint32_t kClear     = 0x1000002Au;
constexpr uint32_t kMask      = 0x1000002Cu;
constexpr uint32_t kCode      = 0x1000002Eu;

constexpr uint16_t kLatchModes[] = {0x40D9u, 0x4051u, 0x0000u};

constexpr uint16_t kNoCode  = 0x00FFu;
constexpr uint16_t kIrlNone = 0x000Fu;
constexpr uint32_t kIrlPins = 4u;

struct SourceCode {
    uint8_t bit;
    uint8_t code;
};

constexpr SourceCode kSourceCodes[] = {
    {1u, 7u}, {2u, 2u}, {3u, 6u}, {5u, 12u}, {7u, 3u}, {8u, 13u}, {9u, 14u}, {10u, 8u}, {12u, 11u}, {14u, 4u}, {15u, 9u},
};

constexpr Sh77xxIntcPin kIrlPin[kIrlPins] = {Sh77xxIntcPin::kIrq0, Sh77xxIntcPin::kIrq1, Sh77xxIntcPin::kIrq2,
                                             Sh77xxIntcPin::kIrq3};

constexpr uint8_t kInputSourceBit[] = {2u, 3u, 5u, 5u, 7u, 9u, 9u, 2u, 3u, 5u, 7u, 8u, 9u, 10u, 14u, 9u, 1u};
static_assert(std::size(kInputSourceBit) == static_cast<size_t>(CasioCassiopeiaA10ChipInput::kCount));
static_assert(std::size(kInputSourceBit) <= 32u);

}

bool CasioCassiopeiaA10ChipIntc::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
}

void CasioCassiopeiaA10ChipIntc::OnReady() {
    intc_ = &emu_.Get<Sh77xxIntc>();
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Reset(); });
    std::lock_guard<std::mutex> lk(mtx_);
    driven_irl_ = static_cast<uint16_t>(~kIrlNone);
    DriveIrlLocked();
}

uint32_t CasioCassiopeiaA10ChipIntc::MmioBase() const { return kLatchMode; }
uint32_t CasioCassiopeiaA10ChipIntc::MmioSize() const { return kCode + 2u - kLatchMode; }

uint16_t CasioCassiopeiaA10ChipIntc::ReadHalf(uint32_t addr) {
    std::lock_guard<std::mutex> lk(mtx_);
    switch (addr) {
    case kLatchMode:
        return latch_mode_;
    case kClear:
        return latched_;
    case kMask:
        return mask_;
    case kCode: {
        const uint16_t code = CodeLocked();
        return code == kNoCode ? kIrlNone : code;
    }
    default:
        HaltUnsupportedAccess("ReadHalf", addr, 0u);
    }
}

void CasioCassiopeiaA10ChipIntc::WriteHalf(uint32_t addr, uint16_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    switch (addr) {
    case kLatchMode:
        if (std::ranges::find(kLatchModes, value) == std::ranges::end(kLatchModes)) {
            HaltUnsupportedAccess("WriteHalf", addr, value);
        }
        latch_mode_ = value;
        break;
    case kClear:
        latched_ = static_cast<uint16_t>(latched_ & ~value);
        break;
    case kMask:
        mask_ = value;
        break;
    default:
        HaltUnsupportedAccess("WriteHalf", addr, value);
    }
    DriveIrlLocked();
}

void CasioCassiopeiaA10ChipIntc::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(mtx_);
    w.Write("chip_input_lines", input_lines_);
    w.Write("latch_mode", latch_mode_);
    w.Write("latched", latched_);
    w.Write("mask", mask_);
}

void CasioCassiopeiaA10ChipIntc::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(mtx_);
    r.Read("chip_input_lines", input_lines_);
    r.Read("latch_mode", latch_mode_);
    r.Read("latched", latched_);
    r.Read("mask", mask_);
}

void CasioCassiopeiaA10ChipIntc::PostRestore() {
    std::lock_guard<std::mutex> lk(mtx_);
    driven_irl_ = static_cast<uint16_t>(~kIrlNone);
    DriveIrlLocked();
}

void CasioCassiopeiaA10ChipIntc::SetInputLevel(CasioCassiopeiaA10ChipInput input, bool asserted) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint16_t before = SourceLevelsLocked();
    const uint32_t line   = 1u << static_cast<uint32_t>(input);
    input_lines_ = asserted ? input_lines_ | line : input_lines_ & ~line;
    const uint16_t rising = static_cast<uint16_t>(SourceLevelsLocked() & ~before);
    latched_              = static_cast<uint16_t>(latched_ | (rising & latch_mode_));
    DriveIrlLocked();
    if (rising != 0u) rising_listeners_.Call(rising);
}

uint16_t CasioCassiopeiaA10ChipIntc::SourceLevels() {
    std::lock_guard<std::mutex> lk(mtx_);
    return SourceLevelsLocked();
}

uint16_t CasioCassiopeiaA10ChipIntc::SourceLevelsLocked() const {
    uint16_t levels = 0u;
    for (uint32_t input = 0; input < std::size(kInputSourceBit); ++input) {
        if (((input_lines_ >> input) & 1u) != 0u) levels = static_cast<uint16_t>(levels | (1u << kInputSourceBit[input]));
    }
    return levels;
}

void CasioCassiopeiaA10ChipIntc::AddRisingEdgeListener(CallbackList<uint16_t>::Callback listener) {
    std::lock_guard<std::mutex> lk(mtx_);
    rising_listeners_.Add(std::move(listener));
}

uint16_t CasioCassiopeiaA10ChipIntc::RequestsLocked() const {
    const uint16_t pending = static_cast<uint16_t>((latched_ & latch_mode_) | (SourceLevelsLocked() & ~latch_mode_));
    return static_cast<uint16_t>(pending & mask_);
}

uint16_t CasioCassiopeiaA10ChipIntc::CodeLocked() const {
    const uint16_t requests = RequestsLocked();
    uint16_t       best     = kNoCode;
    for (const SourceCode& source : kSourceCodes) {
        if (((requests >> source.bit) & 1u) != 0u && source.code < best) best = source.code;
    }
    return best;
}

void CasioCassiopeiaA10ChipIntc::DriveIrlLocked() {
    const uint16_t code = CodeLocked();
    const uint16_t irl  = code == kNoCode ? kIrlNone : code;
    if (irl == driven_irl_) return;
    driven_irl_ = irl;
    for (uint32_t n = 0; n < kIrlPins; ++n) intc_->SetPinLevel(kIrlPin[n], ((irl >> n) & 1u) != 0u);
}

void CasioCassiopeiaA10ChipIntc::Reset() {
    std::lock_guard<std::mutex> lk(mtx_);
    latch_mode_ = 0u;
    latched_    = 0u;
    mask_       = 0u;
    DriveIrlLocked();
}

}
