#include "hd64461_system_control.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../boards/hp_600/hp_600_family_id.h"
#include "../peripheral_dispatcher.h"
#include "hd64461_gpio.h"
#include "hd64461_lcdc.h"
#include "hd64461_pcc.h"
#include "hd64461_socket_wiring.h"
#include "hd64461_timer.h"

#include <algorithm>
#include <iterator>

namespace cerfcore {

REGISTER_SERVICE(Hd64461SystemControl);

namespace {

constexpr uint32_t kStbcr  = 0u;
constexpr uint32_t kSyscr  = 1u;
constexpr uint32_t kScpucr = 2u;

constexpr uint16_t kCkioStby    = 0x2000u;
constexpr uint16_t kCkioModules = 0x19FFu;

constexpr uint16_t kSlckeIst   = 0x0800u;
constexpr uint16_t kSlckeOst   = 0x0200u;
constexpr uint16_t kSlcdst     = 0x0080u;
constexpr uint16_t kSpc0st     = 0x0040u;
constexpr uint16_t kSpc1st     = 0x0020u;
constexpr uint16_t kStm0st     = 0x0008u;
constexpr uint16_t kStm1st     = 0x0004u;
constexpr uint16_t kLcdcClock  = kSlckeIst | kSlckeOst | kSlcdst;
constexpr uint16_t kTimerClock = kStm0st | kStm1st;

constexpr uint16_t kScpuBusIgat = 0x2000u;

struct GatedStore {
    uint32_t reg;
    uint16_t value;
};

constexpr GatedStore kGatedStores[] = {
    {kScpucr, 0x55B2u},
    {kStbcr, 0xFFFFu},
    {kStbcr, 0x3FFFu},
};

constexpr uint32_t kGatedRegistersBase = 0x10000006u;
constexpr uint32_t kGatedRegistersEnd  = 0x12000000u;
constexpr uint32_t kCardSpaceBase      = 0x14000000u;
constexpr uint32_t kCardSpaceEnd       = 0x1C000000u;

constexpr uint16_t kSptaIr = 0x0080u;
constexpr uint16_t kSptaTm = 0x0040u;
constexpr uint16_t kSptbUr = 0x0020u;

constexpr uint32_t kGatingBit0 = 8u;

uint32_t GatedPorts(uint16_t scpucr) {
    uint32_t ports = 0u;
    for (uint32_t port = 0; port < cerf_hd64461_gpio_detail::kPorts; ++port) {
        if (((scpucr >> (kGatingBit0 + 2u * port)) & 1u) != 0u) ports |= 1u << port;
    }
    return ports;
}

}

bool Hd64461SystemControl::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
}

void Hd64461SystemControl::OnReady() {
    wiring_          = &emu_.Get<Hd64461SocketWiring>();
    auto& dispatcher = emu_.Get<PeripheralDispatcher>();
    register_gate_   = dispatcher.InstallBusGate(kGatedRegistersBase, kGatedRegistersEnd);
    card_gate_       = dispatcher.InstallBusGate(kCardSpaceBase, kCardSpaceEnd);
    RegWindow::OnReady();
    std::lock_guard<std::mutex> lk(lock_);
    PublishLocked();
}

void Hd64461SystemControl::RestoreState(StateReader& r) {
    RegWindow::RestoreState(r);
    std::lock_guard<std::mutex> lk(lock_);
    PublishLocked();
}

uint8_t Hd64461SystemControl::ReadByte(uint32_t addr) {
    PassCpuBus("ReadByte", addr, 0u, false);
    return RegWindow::ReadByte(addr);
}

uint16_t Hd64461SystemControl::ReadHalf(uint32_t addr) {
    PassCpuBus("ReadHalf", addr, 0u, addr - kHd64461SystemControlModel.base == 2u * kSyscr);
    return RegWindow::ReadHalf(addr);
}

void Hd64461SystemControl::WriteByte(uint32_t addr, uint8_t value) {
    PassCpuBus("WriteByte", addr, value, false);
    RegWindow::WriteByte(addr, value);
}

void Hd64461SystemControl::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t off       = addr - kHd64461SystemControlModel.base;
    const auto     accepted  = [&](const GatedStore& s) { return off == 2u * s.reg && value == s.value; };
    const bool     unchanged = off == 2u * kSyscr && value == syscr_.load(std::memory_order_acquire);
    PassCpuBus("WriteHalf", addr, value,
               unchanged || std::any_of(std::begin(kGatedStores), std::end(kGatedStores), accepted));
    RegWindow::WriteHalf(addr, value);
}

void Hd64461SystemControl::PassCpuBus(const char* access, uint32_t addr, uint32_t value, bool gated_store) const {
    if (!CpuBusInputsGated() || gated_store) return;
    emu_.Get<Fatal>().Die("Hd64461SystemControl: %s at 0x%08X value 0x%04X while SYSCR SCPU_BUS_IGAT gates the CPU bus",
                          access, addr, value);
}

bool Hd64461SystemControl::CpuBusInputsGated() const {
    return (syscr_.load(std::memory_order_acquire) & kScpuBusIgat) != 0u;
}

bool Hd64461SystemControl::LcdcClockRunning() const {
    return (stbcr_.load(std::memory_order_acquire) & kLcdcClock) == 0u;
}

bool Hd64461SystemControl::PccChannelClockRunning(uint32_t channel) const {
    return (stbcr_.load(std::memory_order_acquire) & (channel == 0u ? kSpc0st : kSpc1st)) == 0u;
}

bool Hd64461SystemControl::TimerClockRunning(uint32_t channel) const {
    return (stbcr_.load(std::memory_order_acquire) & (channel == 0u ? kStm0st : kStm1st)) == 0u;
}

bool Hd64461SystemControl::TimerOutputsSelected() const {
    return (syscr_.load(std::memory_order_acquire) & kSptaTm) != 0u;
}

uint16_t Hd64461SystemControl::SampleRegister(uint32_t i, uint32_t addr) {
    (void)addr;
    return static_cast<uint16_t>(StoredReg(i) | wiring_->ModePins());
}

void Hd64461SystemControl::BeforeStore(uint32_t i, uint16_t value) {
    if (i == kSyscr && (value & (kSptaIr | kSptbUr)) == (kSptaIr | kSptbUr)) {
        emu_.Get<Fatal>().Die("Hd64461SystemControl: SYSCR write 0x%04X selects UART and IrDA together", value);
    }
    if (i == kStbcr && (value & kCkioStby) != 0u && (value & kCkioModules) != kCkioModules) {
        emu_.Get<Fatal>().Die("Hd64461SystemControl: STBCR write 0x%04X gates CKIO while a module runs", value);
    }
}

void Hd64461SystemControl::AfterStore(uint32_t i) {
    const uint16_t before       = stbcr_.load(std::memory_order_relaxed);
    const uint16_t syscr_before = syscr_.load(std::memory_order_relaxed);
    PublishLocked();
    if (((syscr_before ^ StoredReg(kSyscr)) & kScpuBusIgat) != 0u) emu_.Get<Hd64461Lcdc>().OnModuleClockChanged();
    if (i == kScpucr) emu_.Get<Hd64461Pcc>().ApplyInputGating(GatedPorts(StoredReg(kScpucr)));
    if (i != kStbcr) return;
    if (((before ^ StoredReg(kStbcr)) & kLcdcClock) != 0u) emu_.Get<Hd64461Lcdc>().OnModuleClockChanged();
    if (((before ^ StoredReg(kStbcr)) & kTimerClock) != 0u) emu_.Get<Hd64461Timer>().OnModuleClockChanged();
}

void Hd64461SystemControl::AfterReset() {
    const bool was_gated = CpuBusInputsGated();
    PublishLocked();
    if (was_gated) emu_.Get<Hd64461Lcdc>().OnModuleClockChanged();
}

void Hd64461SystemControl::PublishLocked() {
    stbcr_.store(StoredReg(kStbcr), std::memory_order_release);
    syscr_.store(StoredReg(kSyscr), std::memory_order_release);
    ApplyCpuBusGateLocked();
}

void Hd64461SystemControl::ApplyCpuBusGateLocked() {
    auto&      dispatcher = emu_.Get<PeripheralDispatcher>();
    const bool gated      = CpuBusInputsGated();
    dispatcher.SetBusGate(register_gate_, gated);
    dispatcher.SetBusGate(card_gate_, gated);
}

}
