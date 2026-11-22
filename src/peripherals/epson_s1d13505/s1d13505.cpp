#include "s1d13505.h"

#include "s1d13505_config.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../peripheral_dispatcher.h"

#include <cstring>

namespace cerfcore {

namespace {

constexpr uint8_t  kRevisionCode         = 0x0Cu;
constexpr uint8_t  kLcdEnable            = 0x01u;
constexpr uint8_t  kSimultaneousOption   = 0x60u;
constexpr uint8_t  kLcdPowerDisable      = 0x08u;
constexpr uint8_t  kPowerSaveStatus      = 0x80u;
constexpr uint8_t  kHostInterfaceDisable = 0x80u;
constexpr uint16_t kOneMegDramPin        = 0x0040u;
constexpr uint32_t k256Kx16Bytes         = 0x80000u;
constexpr uint32_t k1Mx16Bytes           = 0x200000u;
constexpr uint32_t kSwivelLine           = 1024u;
constexpr uint32_t kBppBySelect[8]       = {1u, 2u, 4u, 8u, 15u, 16u, 0u, 0u};

}

bool S1d13505::ShouldRegister() {
    return emu_.TryGet<S1d13505Config>() != nullptr;
}

void S1d13505::OnReady() {
    config_    = &emu_.Get<S1d13505Config>();
    mmio_base_ = config_->HostWindowBase();
    pins_      = config_->ConfigurationPins();
    buffer_.Allocate((pins_ & kOneMegDramPin) != 0u ? k1Mx16Bytes : k256Kx16Bytes);
    ResetRegisters();
    AttachScanClock();
    config_->RegisterClkiListener([this] { OnScanSourceClockChange(); });
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { HardwareReset(); });
    emu_.Get<PeripheralDispatcher>().Register(this);
}

void S1d13505::HardwareReset() {
    {
        std::lock_guard<std::mutex> lk(state_mtx_);
        if (ScanLiveLocked()) StopScanLocked();
        ResetRegisters();
        lut_port_.ResetPointer();
    }
    mode_latch_.Publish(emu_, false, 0u, 0u);
}

void S1d13505::ResetRegisters() {
    std::memset(reg_, 0, sizeof(reg_));
    reg_[0x0E] = 0xFFu;
    reg_[0x0F] = 0x03u;
    reg_[0x1B] = kHostInterfaceDisable;
}

uint32_t S1d13505::Bpp() const {
    return kBppBySelect[(Reg(0x0D) >> 2) & 0x7u];
}

uint32_t S1d13505::PanMask() const {
    switch (Bpp()) {
        case 1u: return 0xFu;
        case 2u: return 0x7u;
        case 4u: return 0x3u;
        case 8u: return 0x1u;
        default: return 0x0u;
    }
}

uint8_t S1d13505::GpioPins() const {
    switch ((pins_ >> 6) & 0x3u) {
        case 0x0u: return 0x0Eu;
        case 0x3u: return 0x00u;
        default:   return 0x06u;
    }
}

uint32_t S1d13505::DirectXrgb(uint16_t px, uint32_t bpp) const {
    if (Tft()) return bpp == 16u ? lcd_pixel::Expand565(px) : lcd_pixel::Expand555(px);
    const uint32_t g4 = bpp == 16u ? (px >> 7) & 0xFu : (px >> 6) & 0xFu;
    if (!Color()) return lcd_pixel::PackXrgb4(g4, g4, g4);
    if (bpp == 16u) return lcd_pixel::PackXrgb4(px >> 12, g4, (px >> 1) & 0xFu);
    return lcd_pixel::PackXrgb4((px >> 11) & 0xFu, g4, (px >> 1) & 0xFu);
}

bool S1d13505::InkCursor(EpsonInkCursorLayer& layer) const {
    const uint32_t mode = Reg(0x27) >> 6;
    if (mode == 0u) return false;
    const bool cursor = mode == 1u;
    layer.x          = cursor ? static_cast<int32_t>(TenBits(0x28)) : 0;
    layer.y          = cursor ? static_cast<int32_t>(TenBits(0x2A)) : 0;
    layer.width      = cursor ? 64u : PanelWidth();
    layer.height     = cursor ? 64u : PanelHeight();
    layer.start_byte = EpsonInkCursorLayer::StartByte(buffer_.Size(), Reg(0x30));
    layer.line_bytes = cursor ? 16u : ((Reg(0x04) & 0x7Fu) + 1u) * 2u;
    layer.color0     = DirectXrgb(static_cast<uint16_t>((Reg(0x2D) << 8) | Reg(0x2C)), 16u);
    layer.color1     = DirectXrgb(static_cast<uint16_t>((Reg(0x2F) << 8) | Reg(0x2E)), 16u);
    return true;
}

bool S1d13505::PanelPowered() const {
    return (Reg(0x0D) & kLcdEnable) != 0u && (Reg(0x1A) & kLcdPowerDisable) == 0u && !SoftwareSuspend();
}

void S1d13505::CheckOutputFormat() const {
    if ((Reg(0x0D) & kLcdEnable) == 0u) return;
    if (((Reg(0x02) >> 4) & 0x3u) == 0x3u) {
        emu_.Get<Fatal>().Die("S1d13505: REG[02h]=0x%02X selects the reserved panel data width", Reg(0x02));
    }
    if (Bpp() == 0u) {
        emu_.Get<Fatal>().Die("S1d13505: REG[0Dh]=0x%02X selects a reserved color depth", Reg(0x0D));
    }
}

uint64_t S1d13505::LineTicks() const {
    return PanelWidth() + ((Reg(0x05) & 0x1Fu) + 1u) * 8u;
}

GuestCycleClock::Rate S1d13505::PixelRate() const {
    GuestCycleClock::Rate rate = config_->ClkiRate();
    if ((Reg(0x19) & 0x04u) != 0u) rate.den *= 2u;
    rate.den *= (Reg(0x19) & 0x3u) + 1u;
    return rate;
}

RasterScanPeripheral::ScanShape S1d13505::ScanShapeLocked() const {
    ScanShape s;
    s.tick          = PixelRate();
    s.frame.ticks   = LineTicks() * (DisplayLines() + (Reg(0x0A) & 0x3Fu) + 1u);
    s.frame.edges   = 1u;
    s.frame.edge[0] = s.frame.ticks;
    return s;
}

void S1d13505::RetimeScanLocked() {
    if (ScanLiveLocked()) StopScanLocked();
    if (ScanRunning()) StartScanLocked(ScanNow());
}

uint8_t S1d13505::VndStatusBit() {
    std::lock_guard<std::mutex> lk(state_mtx_);
    if (!ScanLiveLocked()) return 0x00u;
    const uint64_t tick = ScanTickInFrameLocked(ScanNow());
    return tick >= LineTicks() * DisplayLines() ? 0x80u : 0x00u;
}

void S1d13505::PublishOnPanelEdge() {
    const bool on = PanelPowered();
    if (!mode_latch_.Publish(emu_, on, on ? PanelWidth() : 0u, on ? PanelHeight() : 0u)) return;
    LOG(Lcd, "S1d13505: panel on=%u %ux%u %ubpp swivel=%u\n", on ? 1u : 0u, PanelWidth(), PanelHeight(), Bpp(),
        Swivel() ? 1u : 0u);
}

uint8_t S1d13505::RegRead(uint32_t addr, uint32_t off) {
    if (HostInterfaceDisabled() && off != 0x1Au && off != 0x1Bu) {
        emu_.Get<Fatal>().Die("S1d13505: REG[%02Xh] read at 0x%08X with the host interface disabled", off, addr);
    }
    switch (off) {
        case 0x00:
            return kRevisionCode;
        case 0x0A:
            return static_cast<uint8_t>((reg_[off] & 0x7Fu) | VndStatusBit());
        case 0x1A:
            return static_cast<uint8_t>((reg_[off] & 0x7Fu) | (SoftwareSuspend() ? kPowerSaveStatus : 0u));
        case 0x1C:
            return static_cast<uint8_t>(pins_);
        case 0x1D:
            return static_cast<uint8_t>(pins_ >> 8);
        case 0x20: {
            const uint8_t outputs = reg_[0x1E] & GpioPins();
            const uint8_t inputs  = config_->GpioInputLevels() & GpioPins() & static_cast<uint8_t>(~outputs);
            return static_cast<uint8_t>((reg_[off] & outputs) | inputs);
        }
        case 0x26:
            return lut_port_.Read(lut_table_);
        case 0x25:
            HaltUnsupportedAccess("RegRead", addr, 0u);
        default:
            if (off >= kRegCount) HaltUnsupportedAccess("RegRead", addr, 0u);
            return reg_[off];
    }
}

void S1d13505::RegWrite(uint32_t addr, uint32_t off, uint8_t value) {
    if (HostInterfaceDisabled() && off != 0x1Bu) {
        emu_.Get<Fatal>().Die("S1d13505: REG[%02Xh]=0x%02X written at 0x%08X with the host interface disabled", off,
                              value, addr);
    }
    RegStore(addr, off, value);
    PublishOnPanelEdge();
}

void S1d13505::RegStore(uint32_t addr, uint32_t off, uint8_t value) {
    switch (off) {
        case 0x00: case 0x1C: case 0x1D:
            return;
        case 0x0D:
            if ((value & kSimultaneousOption) != 0u) {
                emu_.Get<Fatal>().Die("S1d13505: REG[0Dh]=0x%02X selects a simultaneous display mode", value);
            }
            WriteTiming(off, value);
            return;
        case 0x19:
            if ((value & 0x80u) != 0u) emu_.Get<Fatal>().Die("S1d13505: REG[19h]=0x%02X sets reserved bit 7", value);
            WriteTiming(off, value);
            return;
        case 0x0A: case 0x1A:
            WriteTiming(off, static_cast<uint8_t>(value & 0x7Fu));
            return;
        case 0x02: case 0x04: case 0x05: case 0x08: case 0x09:
            WriteTiming(off, value);
            return;
        case 0x25:
            HaltUnsupportedAccess("RegWrite", addr, value);
        case 0x22:
            if ((value & 0x83u) != 0u || ((value >> 5) & 0x3u) == 0x3u || ((value >> 2) & 0x3u) == 0x3u) {
                emu_.Get<Fatal>().Die("S1d13505: REG[22h]=0x%02X sets a reserved field", value);
            }
            break;
        case 0x23:
            if (((value >> 5) & 0x3u) >= 0x2u) {
                emu_.Get<Fatal>().Die("S1d13505: REG[23h]=0x%02X selects an undefined wait-state setting", value);
            }
            break;
        case 0x24:
            lut_port_.PointAt(value);
            break;
        case 0x26: {
            EpsonLutTable* const targets[] = {&lut_table_};
            lut_port_.Write(value, targets);
            return;
        }
        case 0x27:
            if ((value & 0xC0u) == 0xC0u) {
                emu_.Get<Fatal>().Die("S1d13505: REG[27h]=0x%02X selects the reserved ink/cursor mode", value);
            }
            break;
        case 0x29: case 0x2B:
            if ((value & 0x80u) != 0u) {
                emu_.Get<Fatal>().Die("S1d13505: REG[%02Xh]=0x%02X sets reserved bit 7", off, value);
            }
            break;
        default:
            if (off >= kRegCount) HaltUnsupportedAccess("RegWrite", addr, value);
            break;
    }
    reg_[off] = value;
}

void S1d13505::WriteTiming(uint32_t off, uint8_t value) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    const uint8_t old = reg_[off];
    reg_[off] = value;
    CheckOutputFormat();
    if (old != value) RetimeScanLocked();
}

void S1d13505::RequireMemoryAccess(uint32_t addr, uint32_t value) const {
    if (HostInterfaceDisabled()) {
        emu_.Get<Fatal>().Die("S1d13505: display buffer access at 0x%08X (value 0x%08X) with the host interface "
                              "disabled", addr, value);
    }
    if (SoftwareSuspend()) {
        emu_.Get<Fatal>().Die("S1d13505: display buffer access at 0x%08X (value 0x%08X) in software suspend "
                              "REG[1Ah]=0x%02X", addr, value, Reg(0x1A));
    }
}

uint32_t S1d13505::SwivelOffset(uint32_t addr, uint32_t off) const {
    if (Bpp() == 16u) {
        const uint32_t line  = off >> 11;
        const uint32_t pixel = (off >> 1) & (kSwivelLine - 1u);
        return (pixel << 11) | ((kSwivelLine - 1u - line) << 1) | (off & 1u);
    }
    if (Bpp() == 8u && off < kSwivelLine * kSwivelLine) {
        const uint32_t line  = off >> 10;
        const uint32_t pixel = off & (kSwivelLine - 1u);
        return (pixel << 10) | (kSwivelLine - 1u - line);
    }
    emu_.Get<Fatal>().Die("S1d13505: SwivelView access at 0x%08X with REG[0Dh]=0x%02X", addr, Reg(0x0D));
}

void S1d13505::RequireOutsideHalfFrameBuffer(uint32_t addr, uint32_t off, uint32_t width) const {
    if (!DualPanel() || (Reg(0x1B) & 0x01u) != 0u || (Reg(0x0D) & kLcdEnable) == 0u) return;
    const uint32_t bytes = PanelWidth() * PanelHeight() * (Color() ? 4u : 1u) / 16u;
    if (buffer_.Wrap(off) + width <= buffer_.Size() - bytes) return;
    emu_.Get<Fatal>().Die("S1d13505: read at 0x%08X of the half frame buffer, the top %u bytes of the display "
                          "buffer in dual-panel mode", addr, bytes);
}

uint32_t S1d13505::MemoryLoad(uint32_t addr, uint32_t off, uint32_t width) {
    RequireMemoryAccess(addr, 0u);
    if (!Swivel()) {
        RequireOutsideHalfFrameBuffer(addr, off, width);
        return buffer_.Load(off, width);
    }
    uint32_t v = 0;
    for (uint32_t i = 0; i < width; ++i) {
        const uint32_t at = SwivelOffset(addr, off + i);
        RequireOutsideHalfFrameBuffer(addr, at, 1u);
        v |= buffer_.Load(at, 1u) << (8u * i);
    }
    return v;
}

void S1d13505::MemoryStore(uint32_t addr, uint32_t off, uint32_t value, uint32_t width) {
    RequireMemoryAccess(addr, value);
    if (!Swivel()) {
        buffer_.Store(off, value, width);
        return;
    }
    for (uint32_t i = 0; i < width; ++i) buffer_.Store(SwivelOffset(addr, off + i), value >> (8u * i), 1u);
}

uint8_t S1d13505::ReadByte(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (off >= kBufferBase) return static_cast<uint8_t>(MemoryLoad(addr, off - kBufferBase, 1u));
    return RegRead(addr, off & kRegDecode);
}

uint16_t S1d13505::ReadHalf(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (off >= kBufferBase) return static_cast<uint16_t>(MemoryLoad(addr, off - kBufferBase, 2u));
    return static_cast<uint16_t>(RegRead(addr, off & kRegDecode) | (RegRead(addr + 1u, (off + 1u) & kRegDecode) << 8));
}

uint32_t S1d13505::ReadWord(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (off >= kBufferBase) return MemoryLoad(addr, off - kBufferBase, 4u);
    uint32_t v = 0;
    for (uint32_t i = 0; i < 4u; ++i) v |= uint32_t(RegRead(addr + i, (off + i) & kRegDecode)) << (8u * i);
    return v;
}

void S1d13505::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off = addr - MmioBase();
    if (off >= kBufferBase) {
        MemoryStore(addr, off - kBufferBase, value, 1u);
        return;
    }
    RegWrite(addr, off & kRegDecode, value);
}

void S1d13505::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t off = addr - MmioBase();
    if (off >= kBufferBase) {
        MemoryStore(addr, off - kBufferBase, value, 2u);
        return;
    }
    RegWrite(addr, off & kRegDecode, static_cast<uint8_t>(value));
    RegWrite(addr + 1u, (off + 1u) & kRegDecode, static_cast<uint8_t>(value >> 8));
}

void S1d13505::WriteWord(uint32_t addr, uint32_t value) {
    const uint32_t off = addr - MmioBase();
    if (off >= kBufferBase) {
        MemoryStore(addr, off - kBufferBase, value, 4u);
        return;
    }
    for (uint32_t i = 0; i < 4u; ++i) RegWrite(addr + i, (off + i) & kRegDecode, static_cast<uint8_t>(value >> (8u * i)));
}

void S1d13505::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    w.WriteBytes("reg", reg_, sizeof(reg_));
    buffer_.SaveState(w, "vram");
    lut_table_.SaveState(w, "lut");
    lut_port_.SaveState(w);
    mode_latch_.SaveState(w);
    SaveScanLocked(w);
}

void S1d13505::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    r.ReadBytes("reg", reg_, sizeof(reg_));
    buffer_.RestoreState(r, "vram");
    lut_table_.RestoreState(r, "lut");
    lut_port_.RestoreState(r);
    mode_latch_.RestoreState(r);
    RestoreScanLocked(r);
}

void S1d13505::PostRestore() {
    std::lock_guard<std::mutex> lk(state_mtx_);
    ResumeScanLocked(ScanNow());
}

REGISTER_SERVICE(S1d13505);

}
