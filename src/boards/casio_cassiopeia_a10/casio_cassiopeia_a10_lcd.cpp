#include "casio_cassiopeia_a10_lcd.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../socs/sh77xx/sh77xx_area3_dram.h"
#include "casio_cassiopeia_a10_family_id.h"

#include <algorithm>

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10Lcd);

namespace {

constexpr uint32_t kControl       = 0x10000140u;
constexpr uint32_t kFirstSetup    = 0x10000142u;
constexpr uint32_t kFirstOutput   = 0x10000154u;
constexpr uint32_t kBlockEnd      = 0x1000015Au;
constexpr uint16_t kDisplayOn     = 0x0002u;
constexpr uint16_t kModeMask      = 0xFFFCu;
constexpr uint16_t kKernelMode    = 0x9304u;
constexpr uint16_t kGwesMode      = 0xD000u;
constexpr uint16_t kMonitorMode   = 0xE000u;
constexpr uint32_t kPixelsPerWord = 8u;
constexpr uint32_t kBitsPerPixel  = 2u;
constexpr uint32_t kLevels        = 1u << kBitsPerPixel;

struct SetupValues {
    uint8_t  count;
    uint16_t values[3];
};

constexpr SetupValues kSetupValues[] = {
    {1u, {0x0000u}},
    {1u, {0x0000u}},
    {1u, {0x003Cu}},
    {2u, {0x0078u, 0x0000u}},
    {3u, {0x293Eu, 0x3131u, 0x313Cu}},
    {3u, {0x0029u, 0x003Du, 0x0031u}},
    {0u, {}},
    {0u, {}},
    {1u, {0x4B00u}},
};

}

bool CasioCassiopeiaA10Lcd::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
}

void CasioCassiopeiaA10Lcd::OnReady() {
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Reset(); });
}

uint32_t CasioCassiopeiaA10Lcd::MmioBase() const { return kControl; }
uint32_t CasioCassiopeiaA10Lcd::MmioSize() const { return kBlockEnd - kControl; }

uint8_t CasioCassiopeiaA10Lcd::ReadByte(uint32_t addr) {
    if (addr < kFirstOutput) HaltUnsupportedAccess("ReadByte", addr, 0u);
    return PlaneRead(addr);
}

uint16_t CasioCassiopeiaA10Lcd::ReadHalf(uint32_t addr) {
    if (addr >= kFirstOutput) return PlaneRead(addr);
    if (addr != kControl) HaltUnsupportedAccess("ReadHalf", addr, 0u);
    return control_.load(std::memory_order_acquire);
}

void CasioCassiopeiaA10Lcd::WriteHalf(uint32_t addr, uint16_t value) {
    if (addr == kControl) {
        control_.store(value, std::memory_order_release);
        size_latch_.PublishOnce(emu_, PanelOn());
        return;
    }
    if (addr == kFirstOutput) {
        const uint32_t planes = Planes();
        if (planes == 0u || !Convert(value, planes)) HaltUnsupportedAccess("WriteHalf", addr, value);
        return;
    }
    const uint32_t index = (addr - kFirstSetup) / 2u;
    if (addr < kFirstSetup || (addr & 1u) != 0u || index >= kSetupRegisters) {
        HaltUnsupportedAccess("WriteHalf", addr, value);
    }
    const SetupValues& accepted = kSetupValues[index];
    const uint16_t*    last     = accepted.values + accepted.count;
    if (std::find(accepted.values, last, value) == last) HaltUnsupportedAccess("WriteHalf", addr, value);
    setup_[index] = value;
}

void CasioCassiopeiaA10Lcd::SaveState(StateWriter& w) {
    w.Write("control", control_.load(std::memory_order_acquire));
    w.Write("setup", setup_);
    w.Write("outputs", outputs_);
    w.Write("converted_planes", converted_planes_);
    size_latch_.SaveState(w);
}

void CasioCassiopeiaA10Lcd::RestoreState(StateReader& r) {
    uint16_t control = 0u;
    r.Read("control", control);
    control_.store(control, std::memory_order_release);
    r.Read("setup", setup_);
    r.Read("outputs", outputs_);
    r.Read("converted_planes", converted_planes_);
    size_latch_.RestoreState(r);
}

bool CasioCassiopeiaA10Lcd::PanelOn() const {
    return (control_.load(std::memory_order_acquire) & kDisplayOn) != 0u;
}

uint32_t CasioCassiopeiaA10Lcd::Planes() const {
    const uint16_t mode = control_.load(std::memory_order_acquire) & kModeMask;
    if (mode == kKernelMode || mode == kGwesMode) return 2u;
    if (mode == kMonitorMode) return 3u;
    return 0u;
}

uint32_t CasioCassiopeiaA10Lcd::FramePa() const { return kSh77xxArea3PaBase; }

uint8_t CasioCassiopeiaA10Lcd::PlaneRead(uint32_t addr) {
    const uint32_t index = (addr - kFirstOutput) / 2u;
    if ((addr & 1u) != 0u || index >= converted_planes_ || converted_planes_ != Planes()) {
        HaltUnsupportedAccess("PlaneRead", addr, 0u);
    }
    return outputs_[index];
}

bool CasioCassiopeiaA10Lcd::Convert(uint16_t pixels, uint32_t planes) {
    const uint8_t bytes[2] = {static_cast<uint8_t>(pixels), static_cast<uint8_t>(pixels >> 8)};
    uint8_t       out[kMaxPlanes]{};
    for (uint32_t i = 0; i < kPixelsPerWord; ++i) {
        const uint32_t v = lcd_pixel::PackedIndexMsbFirst(bytes, i, kBitsPerPixel);
        if (planes == kMaxPlanes && v != 0u && v != kLevels - 1u) return false;
        for (uint32_t p = 0; p < planes; ++p) {
            const uint32_t bit = planes == kMaxPlanes ? v & 1u : (v >> p) & 1u;
            out[p] = static_cast<uint8_t>(out[p] | (bit << i));
        }
    }
    for (uint32_t p = 0; p < kMaxPlanes; ++p) outputs_[p] = out[p];
    converted_planes_ = static_cast<uint8_t>(planes);
    return true;
}

void CasioCassiopeiaA10Lcd::Reset() {
    control_.store(0u, std::memory_order_release);
    for (uint16_t& v : setup_) v = 0u;
    for (uint8_t& v : outputs_) v = 0u;
    converted_planes_ = 0u;
    size_latch_.Reset();
}

}
