#include "casio_cassiopeia_a10_pin_registers.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "casio_cassiopeia_a10_chip_intc.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_serial_connector.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10PinRegisters);

namespace {

constexpr uint32_t kFirst        = CasioCassiopeiaA10PinRegisters::kFirstPin;
constexpr uint32_t kLast         = CasioCassiopeiaA10PinRegisters::kLastPin;
constexpr uint32_t kPin042       = 0x10000042u;
constexpr uint32_t kPin044       = 0x10000044u;
constexpr uint32_t kPin046       = 0x10000046u;
constexpr uint32_t kPin04A       = 0x1000004Au;
constexpr uint32_t kPin04C       = kCasioCassiopeiaA10Pin04C;
constexpr uint32_t kPin04E       = kCasioCassiopeiaA10Pin04E;
constexpr uint32_t kPin050       = 0x10000050u;
constexpr uint32_t kSerialEdges  = 0x10000052u;
constexpr uint32_t kPin054       = kCasioCassiopeiaA10Pin054;
constexpr uint32_t kPin056       = kCasioCassiopeiaA10Pin056;
constexpr uint32_t kPin058       = kCasioCassiopeiaA10Pin058;
constexpr uint32_t kPin05C       = 0x1000005Cu;
constexpr uint32_t kPin05E       = kCasioCassiopeiaA10Pin05E;
constexpr uint32_t kIoBoxPin     = 0x10000060u;
constexpr uint32_t kPin064       = kCasioCassiopeiaA10Pin064;
constexpr uint32_t kPin066       = kCasioCassiopeiaA10Pin066;
constexpr uint16_t kClearFlags   = 0x0030u;
constexpr uint16_t kConfigBits   = 0x000Fu;
constexpr uint16_t kWordBits     = kConfigBits | kClearFlags;
constexpr uint16_t kFallEnable   = 0x0001u;
constexpr uint16_t kRiseEnable   = 0x0002u;
constexpr uint16_t kFallen       = 0x0010u;
constexpr uint16_t kRisen        = 0x0020u;
constexpr uint16_t kLevelRequest = 0x0008u;
constexpr uint16_t kRequestRead  = 0x0080u;

constexpr uint32_t Index(uint32_t addr) { return (addr - kFirst) / 2u; }

constexpr uint32_t LineBit(uint32_t addr) { return 1u << Index(addr); }

constexpr uint32_t kIdleHighLines = LineBit(kPin042) | LineBit(kPin044) | LineBit(kPin046) | LineBit(kPin04A) |
                                    LineBit(kPin04C) | LineBit(kPin04E) | LineBit(kPin050) | LineBit(kPin054) |
                                    LineBit(kPin056) | LineBit(kPin058) | LineBit(kPin05C) | LineBit(kPin05E) |
                                    LineBit(kIoBoxPin) | LineBit(kPin064) | LineBit(kPin066);

struct PinSource {
    uint32_t                    addr;
    CasioCassiopeiaA10ChipInput input;
};

constexpr PinSource kPinSources[] = {
    {kPin046, CasioCassiopeiaA10ChipInput::kPinBankSource14},
    {kPin04C, CasioCassiopeiaA10ChipInput::kPinBankSource9},
    {kPin04E, CasioCassiopeiaA10ChipInput::kPinBankSource8},
    {kPin050, CasioCassiopeiaA10ChipInput::kPinBankSource8},
    {kSerialEdges, CasioCassiopeiaA10ChipInput::kPinBankSource10},
    {kPin054, CasioCassiopeiaA10ChipInput::kPinBankSource2},
    {kPin056, CasioCassiopeiaA10ChipInput::kPinBankSource3},
    {kPin058, CasioCassiopeiaA10ChipInput::kPinBankSource2},
    {kPin05E, CasioCassiopeiaA10ChipInput::kPinBankSource7},
    {kPin064, CasioCassiopeiaA10ChipInput::kPinBankSource5},
    {kPin066, CasioCassiopeiaA10ChipInput::kPinBankSource5},
};

}

bool CasioCassiopeiaA10PinRegisters::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
}

void CasioCassiopeiaA10PinRegisters::OnReady() {
    chip_intc_ = &emu_.Get<CasioCassiopeiaA10ChipIntc>();
    lines_     = kIdleHighLines;
    emu_.Get<CasioCassiopeiaA10SerialConnector>().AddCarrierListener(
        [this](bool level) { SetLine(kSerialEdges, level); });
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Reset(); });
}

uint32_t CasioCassiopeiaA10PinRegisters::MmioBase() const { return kFirst; }
uint32_t CasioCassiopeiaA10PinRegisters::MmioSize() const { return kLast + 2u - kFirst; }

uint16_t CasioCassiopeiaA10PinRegisters::ReadHalf(uint32_t addr) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint32_t              index = Index(addr);
    return static_cast<uint16_t>(config_[index] | flags_[index] | (LevelRequestLocked(index) ? kRequestRead : 0u));
}

void CasioCassiopeiaA10PinRegisters::WriteHalf(uint32_t addr, uint16_t value) {
    if ((addr & 1u) != 0u || (value & ~(kWordBits | kRequestRead)) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
    std::lock_guard<std::mutex> lk(mtx_);
    const uint32_t              index = Index(addr);
    config_[index]                    = static_cast<uint16_t>(value & kConfigBits);
    flags_[index]                     = static_cast<uint16_t>(flags_[index] & ~(value & kClearFlags));
    DriveSourcesLocked();
}

void CasioCassiopeiaA10PinRegisters::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(mtx_);
    w.Write("config", config_);
    w.Write("flags", flags_);
    w.Write("lines", lines_);
}

void CasioCassiopeiaA10PinRegisters::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(mtx_);
    r.Read("config", config_);
    r.Read("flags", flags_);
    r.Read("lines", lines_);
}

void CasioCassiopeiaA10PinRegisters::PostRestore() {
    std::lock_guard<std::mutex> lk(mtx_);
    DriveSourcesLocked();
}

void CasioCassiopeiaA10PinRegisters::SetLine(uint32_t addr, bool level) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint32_t              index = Index(addr);
    const uint32_t              bit   = 1u << index;
    if (((lines_ & bit) != 0u) == level) return;
    lines_ = level ? lines_ | bit : lines_ & ~bit;
    if ((config_[index] & (level ? kRiseEnable : kFallEnable)) != 0u) {
        flags_[index] = static_cast<uint16_t>(flags_[index] | (level ? kRisen : kFallen));
    }
    DriveSourcesLocked();
}

bool CasioCassiopeiaA10PinRegisters::LevelRequestLocked(uint32_t index) const {
    return (config_[index] & kLevelRequest) != 0u && (lines_ & (1u << index)) == 0u;
}

void CasioCassiopeiaA10PinRegisters::DriveSourcesLocked() {
    for (const PinSource& source : kPinSources) {
        bool requested = false;
        for (const PinSource& pin : kPinSources) {
            const uint32_t index = Index(pin.addr);
            if (pin.input == source.input && (flags_[index] != 0u || LevelRequestLocked(index))) requested = true;
        }
        chip_intc_->SetInputLevel(source.input, requested);
    }
}

void CasioCassiopeiaA10PinRegisters::Reset() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (uint16_t& v : config_) v = 0u;
    for (uint16_t& v : flags_) v = 0u;
    DriveSourcesLocked();
}

}
