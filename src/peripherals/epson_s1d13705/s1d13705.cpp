#include "s1d13705.h"

#include "s1d13705_config.h"
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../peripheral_dispatcher.h"

#include <cstring>

namespace cerfcore {

namespace {

constexpr uint8_t kRevisionCode = 0x24u;

}

bool S1d13705::ShouldRegister() {
    return emu_.TryGet<S1d13705Config>() != nullptr;
}

void S1d13705::OnReady() {
    config_    = &emu_.Get<S1d13705Config>();
    mmio_base_ = config_->HostWindowBase();
    vram_.assign(kDisplayBufferBytes, 0u);
    AttachScanClock();
    config_->RegisterClkiListener([this] { OnClkiChange(); });
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { HardwareReset(); });
    emu_.Get<PeripheralDispatcher>().Register(this);
}

void S1d13705::HardwareReset() {
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        if (ScanLiveLocked()) StopScanLocked();
        std::memset(reg_, 0, sizeof(reg_));
        lut_port_.ResetPointer();
    }
    mode_latch_.Publish(emu_, false, 0u, 0u);
}

uint32_t S1d13705::Bpp() const {
    return 1u << ((Reg(0x02) >> 6) & 0x3u);
}

bool S1d13705::ReservedFormat() const {
    if (Tft()) return false;
    if (!Color() && Bpp() == 8u) return true;
    const uint32_t width = Reg(0x01) & 0x3u;
    if ((Reg(0x01) & 0x40u) != 0u) return width != 0x1u;
    return Color() ? width == 0x2u : width >= 0x2u;
}

void S1d13705::CheckOutputFormat() const {
    if (!DisplayActive()) return;
    if (ReservedFormat()) {
        emu_.Get<Fatal>().Die("S1d13705: REG[01h]=0x%02X REG[02h]=0x%02X select a reserved panel format", Reg(0x01),
                              Reg(0x02));
    }
    if (Tft() && !Tft12Bit()) {
        emu_.Get<Fatal>().Die("S1d13705: REG[01h]=0x%02X selects 9-bit TFT output", Reg(0x01));
    }
}

bool S1d13705::DualPanel() const {
    return !Tft() && (Reg(0x01) & 0x40u) != 0u;
}

uint64_t S1d13705::LineTicks() const {
    const uint64_t line = PanelWidth() + ((Reg(0x08) & 0x1Fu) + 4u) * 8u;
    return DualPanel() ? 2u * line : line;
}

uint32_t S1d13705::DisplayLines() const {
    return DualPanel() ? PanelHeight() / 2u : PanelHeight();
}

uint64_t S1d13705::FrameTicks() const {
    return LineTicks() * (DisplayLines() + (Reg(0x0A) & 0x3Fu));
}

GuestCycleClock::Rate S1d13705::PixelRate() const {
    GuestCycleClock::Rate rate = config_->ClkiRate();
    if ((Reg(0x02) & 0x10u) != 0u) rate.den *= 2u;
    if (Swivel()) {
        const uint32_t select = Reg(0x1B) & 0x3u;
        const bool     alternate = (Reg(0x1B) & 0x40u) != 0u;
        rate.den *= (alternate && select == 0u) ? 2u : (1u << select);
    }
    return rate;
}

RasterScanPeripheral::ScanShape S1d13705::ScanShapeLocked() const {
    if (DualPanel() && (PanelHeight() % 2u) != 0u) {
        emu_.Get<Fatal>().Die("S1d13705: dual panel with an odd vertical panel size %u", PanelHeight());
    }
    ScanShape s;
    s.tick          = PixelRate();
    s.frame.ticks   = FrameTicks();
    s.frame.edges   = 1u;
    s.frame.edge[0] = s.frame.ticks;
    return s;
}

void S1d13705::RetimeScanLocked() {
    if (ScanLiveLocked()) StopScanLocked();
    if (DisplayActive()) StartScanLocked(ScanNow());
}

void S1d13705::OnClkiChange() {
    OnScanSourceClockChange();
}

uint8_t S1d13705::VndStatusBit() {
    std::lock_guard<std::mutex> lk(state_mtx_);
    if (!ScanLiveLocked()) return 0x00u;
    const uint64_t tick = ScanTickInFrameLocked(ScanNow());
    return tick >= LineTicks() * DisplayLines() ? 0x80u : 0x00u;
}

void S1d13705::PublishOnPanelEdge() {
    const bool on = PanelPowered();
    if (!mode_latch_.Publish(emu_, on, on ? PanelWidth() : 0u, on ? PanelHeight() : 0u)) return;
    LOG(Lcd, "S1d13705: panel on %ux%u %ubpp swivel=%u\n", PanelWidth(), PanelHeight(), Bpp(),
        Swivel() ? 1u : 0u);
}

uint8_t S1d13705::RegRead(uint32_t off) {
    switch (off) {
        case 0x00:
            return kRevisionCode;
        case 0x0A:
            return static_cast<uint8_t>((reg_[off] & 0x7Fu) | VndStatusBit());
        case 0x15:
        case 0x17:
            if (!DisplayActive()) {
                emu_.Get<Fatal>().Die("S1d13705: REG[%02Xh] read in power save mode REG[03h]=0x%02X", off, Reg(0x03));
            }
            return off == 0x15u ? reg_[off] : lut_port_.Read(lut_table_);
        case 0x19:
            if ((~reg_[0x18] & 0x1Fu) != 0u) {
                emu_.Get<Fatal>().Die("S1d13705: GPIO status read with input pins (REG[18h]=0x%02X)",
                                      reg_[0x18]);
            }
            return reg_[off];
        case 0x14: case 0x16: case 0x1D: case 0x1E: case 0x1F:
            HaltUnsupportedAccess("RegRead(reserved)", MmioBase() + kRegBase + off, 0u);
        default:
            return reg_[off];
    }
}

void S1d13705::RegWrite(uint32_t off, uint8_t value) {
    switch (off) {
        case 0x00:
            return;
        case 0x15:
            if (!DisplayActive()) return;
            reg_[off] = value;
            lut_port_.PointAt(value);
            return;
        case 0x17: {
            if (!DisplayActive()) return;
            EpsonLutTable* const targets[] = {&lut_table_};
            lut_port_.Write(value, targets);
            return;
        }
        case 0x18:
            if ((value & 0x1Fu) != 0u) {
                emu_.Get<Fatal>().Die("S1d13705: REG[18h]=0x%02X configures GPIO outputs", value);
            }
            reg_[off] = value;
            break;
        case 0x14: case 0x16: case 0x1D: case 0x1E: case 0x1F:
            HaltUnsupportedAccess("RegWrite(reserved)", MmioBase() + kRegBase + off, value);
        case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06:
        case 0x08: case 0x0A: case 0x1B:
            WriteTiming(off, value);
            break;
        default:
            reg_[off] = value;
            break;
    }
    PublishOnPanelEdge();
}

void S1d13705::WriteTiming(uint32_t off, uint8_t value) {
    if (off == 0x02 && (value & 0x02u) != 0u) {
        emu_.Get<Fatal>().Die("S1d13705: REG[02h]=0x%02X enables hardware video invert on FPDAT11", value);
    }
    if (off == 0x03) {
        const uint32_t save = value & 0x3u;
        if (save == 0x1u || save == 0x2u || (value & 0x04u) != 0u) {
            emu_.Get<Fatal>().Die("S1d13705: REG[03h]=0x%02X selects a reserved or board-wired power mode", value);
        }
    }
    if (off == 0x04 && (value & 0x7Fu) < 0x03u) {
        emu_.Get<Fatal>().Die("S1d13705: REG[04h]=0x%02X is below the 03h minimum", value);
    }
    std::lock_guard<std::mutex> lk(state_mtx_);
    const uint8_t old = reg_[off];
    reg_[off] = value;
    CheckOutputFormat();
    if (old != value) RetimeScanLocked();
}

bool S1d13705::IsRegister(uint32_t off, uint32_t width) const {
    return off >= kRegBase && off + width <= kWindowBytes;
}

bool S1d13705::IsBuffer(uint32_t off, uint32_t width) const {
    return off + width <= kDisplayBufferBytes;
}

uint8_t S1d13705::ReadByte(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (IsBuffer(off, 1u)) return vram_[off];
    if (IsRegister(off, 1u)) return RegRead(off - kRegBase);
    HaltUnsupportedAccess("ReadByte", addr, 0u);
}

uint16_t S1d13705::ReadHalf(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (IsBuffer(off, 2u)) return static_cast<uint16_t>(cerf::le::UN(&vram_[off], 2u));
    if (IsRegister(off, 2u)) {
        const uint32_t r = off - kRegBase;
        return static_cast<uint16_t>(RegRead(r) | (RegRead(r + 1u) << 8));
    }
    HaltUnsupportedAccess("ReadHalf", addr, 0u);
}

uint32_t S1d13705::ReadWord(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (IsBuffer(off, 4u)) return static_cast<uint32_t>(cerf::le::UN(&vram_[off], 4u));
    if (IsRegister(off, 4u)) {
        const uint32_t r = off - kRegBase;
        return uint32_t(RegRead(r)) | uint32_t(RegRead(r + 1u)) << 8 | uint32_t(RegRead(r + 2u)) << 16 |
               uint32_t(RegRead(r + 3u)) << 24;
    }
    HaltUnsupportedAccess("ReadWord", addr, 0u);
}

void S1d13705::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off = addr - MmioBase();
    if (IsBuffer(off, 1u)) {
        vram_[off] = value;
        return;
    }
    if (IsRegister(off, 1u)) {
        RegWrite(off - kRegBase, value);
        return;
    }
    HaltUnsupportedAccess("WriteByte", addr, value);
}

void S1d13705::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t off = addr - MmioBase();
    if (IsBuffer(off, 2u)) {
        cerf::le::PutN(&vram_[off], value, 2u);
        return;
    }
    if (IsRegister(off, 2u)) {
        const uint32_t r = off - kRegBase;
        RegWrite(r, static_cast<uint8_t>(value));
        RegWrite(r + 1u, static_cast<uint8_t>(value >> 8));
        return;
    }
    HaltUnsupportedAccess("WriteHalf", addr, value);
}

void S1d13705::WriteWord(uint32_t addr, uint32_t value) {
    const uint32_t off = addr - MmioBase();
    if (IsBuffer(off, 4u)) {
        cerf::le::PutN(&vram_[off], value, 4u);
        return;
    }
    if (IsRegister(off, 4u)) {
        const uint32_t r = off - kRegBase;
        for (uint32_t i = 0; i < 4u; ++i) RegWrite(r + i, static_cast<uint8_t>(value >> (8u * i)));
        return;
    }
    HaltUnsupportedAccess("WriteWord", addr, value);
}

void S1d13705::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    w.WriteBytes("reg", reg_, sizeof(reg_));
    w.WriteBytes("vram", vram_.data(), vram_.size());
    lut_table_.SaveState(w, "lut");
    lut_port_.SaveState(w);
    mode_latch_.SaveState(w);
    SaveScanLocked(w);
}

void S1d13705::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    r.ReadBytes("reg", reg_, sizeof(reg_));
    r.ReadBytes("vram", vram_.data(), vram_.size());
    lut_table_.RestoreState(r, "lut");
    lut_port_.RestoreState(r);
    mode_latch_.RestoreState(r);
    RestoreScanLocked(r);
}

void S1d13705::PostRestore() {
    std::lock_guard<std::mutex> lk(state_mtx_);
    ResumeScanLocked(ScanNow());
}

REGISTER_SERVICE(S1d13705);

}
