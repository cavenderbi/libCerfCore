#include "ite_it8181.h"

#include "ite_it8181_config.h"
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../peripheral_dispatcher.h"

namespace cerfcore {

namespace {

constexpr uint32_t kConfigOffset = 0x007FFC00u;
constexpr uint32_t kGuiBytes     = IteIt8181Gui::kWindowBytes;
constexpr uint32_t kVgaBytes     = 0x00010000u;

constexpr uint32_t kCfgId      = 0x00u;
constexpr uint32_t kCfgClass   = 0x08u;
constexpr uint32_t kCfgMba     = 0x10u;
constexpr uint32_t kCfgGba     = 0x14u;
constexpr uint32_t kCfgSba     = 0x18u;
constexpr uint32_t kCfgTest    = 0x40u;
constexpr uint32_t kCfgStandby = 0x44u;
constexpr uint32_t kCfgPll1    = 0x48u;
constexpr uint32_t kCfgPll2    = 0x4Cu;

constexpr uint32_t kDeviceId  = 0x81811283u;
constexpr uint32_t kClassCode = 0x03800000u;

constexpr uint32_t kMbaDecode = 0x00400000u;
constexpr uint32_t kGbaDecode = 0x007F8000u;
constexpr uint32_t kSbaDecode = 0x007F0000u;

constexpr uint32_t kStandbyClockStop = 0x0001u;
constexpr uint32_t kStandbyDacPower  = 0x0010u;

}

REGISTER_SERVICE(IteIt8181);

bool IteIt8181::ShouldRegister() {
    return emu_.TryGet<IteIt8181Config>() != nullptr;
}

void IteIt8181::OnReady() {
    const IteIt8181Config& cfg = emu_.Get<IteIt8181Config>();
    base_     = cfg.HostWindowBase();
    revision_ = cfg.Revision();
    vram_.assign(kVramBytes, 0u);
    vga_.Attach();
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { PowerOnReset(); });
    emu_.Get<PeripheralDispatcher>().Register(this);
}

void IteIt8181::PowerOnReset() {
    mba_ = gba_ = sba_ = 0u;
    test_ = standby_ = pll1_ = pll2_ = 0u;
    vga_.PowerOnReset();
    gui_.PowerOnReset();
    PublishDisplay();
}

IteIt8181::Target IteIt8181::Decode(uint32_t addr, const char* op, uint64_t value) const {
    const uint32_t off = addr - base_;
    if (off >= kConfigOffset) return Target{Region::Config, off - kConfigOffset};
    if (standby_ & kStandbyClockStop) {
        emu_.Get<Fatal>().Die("IteIt8181: %s at 0x%08X (0x%llX) while STANDBY 0x%08X stops the "
                              "14.318 MHz clock", op, addr, static_cast<unsigned long long>(value),
                              standby_);
    }
    const bool in_vram = off - mba_ < kVramBytes;
    const bool in_gui  = off - gba_ < kGuiBytes;
    const bool in_vga  = off - sba_ < kVgaBytes;
    if (in_vram + in_gui + in_vga != 1) {
        emu_.Get<Fatal>().Die("IteIt8181: %s at 0x%08X (0x%llX) decodes to %d windows (MBA "
                              "0x%06X GBA 0x%06X SBA 0x%06X)", op, addr,
                              static_cast<unsigned long long>(value),
                              in_vram + in_gui + in_vga, mba_, gba_, sba_);
    }
    if (in_vram) return Target{Region::Vram, off - mba_};
    if (in_gui)  return Target{Region::Gui, off - gba_};
    return Target{Region::Vga, off - sba_};
}

uint8_t IteIt8181::ReadByte(uint32_t addr) {
    const Target t = Decode(addr, "byte read", 0u);
    if (t.region == Region::Vram) return vram_[t.off];
    if (t.region == Region::Vga) return vga_.Read(t.off);
    HaltUnsupportedAccess("IteIt8181 byte read", addr, 0u);
}

uint16_t IteIt8181::ReadHalf(uint32_t addr) {
    const Target t = Decode(addr, "halfword read", 0u);
    if (t.region == Region::Vram && t.off + 1u < kVramBytes) return cerf::le::U16(vram_.data(), t.off);
    if (t.region == Region::Vga) {
        const uint8_t lo = vga_.Read(t.off);
        const uint8_t hi = vga_.Read(t.off + 1u);
        return static_cast<uint16_t>(lo | (hi << 8));
    }
    HaltUnsupportedAccess("IteIt8181 halfword read", addr, 0u);
}

uint32_t IteIt8181::ReadWord(uint32_t addr) {
    const Target t = Decode(addr, "word read", 0u);
    switch (t.region) {
        case Region::Config: return ConfigRead(t.off);
        case Region::Gui:    return gui_.Read(t.off);
        case Region::Vram:
            if (t.off + 3u < kVramBytes) return cerf::le::U32(vram_.data(), t.off);
            break;
        case Region::Vga: break;
    }
    HaltUnsupportedAccess("IteIt8181 word read", addr, 0u);
}

void IteIt8181::WriteByte(uint32_t addr, uint8_t value) {
    const Target t = Decode(addr, "byte write", value);
    if (t.region == Region::Vram) {
        vram_[t.off] = value;
        return;
    }
    if (t.region == Region::Vga) {
        vga_.Write(t.off, value);
        PublishDisplay();
        return;
    }
    HaltUnsupportedAccess("IteIt8181 byte write", addr, value);
}

void IteIt8181::WriteHalf(uint32_t addr, uint16_t value) {
    const Target t = Decode(addr, "halfword write", value);
    if (t.region == Region::Vram && t.off + 1u < kVramBytes) {
        cerf::le::Put16(vram_.data() + t.off, value);
        return;
    }
    if (t.region == Region::Vga) {
        vga_.Write(t.off, static_cast<uint8_t>(value));
        vga_.Write(t.off + 1u, static_cast<uint8_t>(value >> 8));
        PublishDisplay();
        return;
    }
    HaltUnsupportedAccess("IteIt8181 halfword write", addr, value);
}

void IteIt8181::WriteWord(uint32_t addr, uint32_t value) {
    const Target t = Decode(addr, "word write", value);
    switch (t.region) {
        case Region::Config: ConfigWrite(t.off, value); return;
        case Region::Gui:    gui_.Write(t.off, value); return;
        case Region::Vram:
            if (t.off + 3u < kVramBytes) {
                cerf::le::Put32(vram_.data() + t.off, value);
                return;
            }
            break;
        case Region::Vga: break;
    }
    HaltUnsupportedAccess("IteIt8181 word write", addr, value);
}

uint32_t IteIt8181::ConfigRead(uint32_t off) {
    switch (off) {
        case kCfgId:      return kDeviceId;
        case kCfgClass:   return kClassCode | revision_;
        case kCfgMba:     return mba_;
        case kCfgGba:     return gba_;
        case kCfgSba:     return sba_;
        case kCfgTest:    return test_;
        case kCfgStandby: return standby_;
        case kCfgPll1:    return pll1_;
        case kCfgPll2:    return pll2_;
        default: break;
    }
    emu_.Get<Fatal>().Die("IteIt8181: read of configuration register 0x%02X", off);
}

void IteIt8181::ConfigWrite(uint32_t off, uint32_t value) {
    switch (off) {
        case kCfgMba:     mba_ = value & kMbaDecode; return;
        case kCfgGba:     gba_ = value & kGbaDecode; return;
        case kCfgSba:     sba_ = value & kSbaDecode; return;
        case kCfgTest:    test_ = value; return;
        case kCfgPll1:    pll1_ = value; return;
        case kCfgPll2:    pll2_ = value; return;
        case kCfgStandby:
            standby_ = value;
            PublishDisplay();
            return;
        default: break;
    }
    emu_.Get<Fatal>().Die("IteIt8181: write 0x%08X to configuration register 0x%02X", value, off);
}

IteIt8181::Scanout IteIt8181::Display() const {
    Scanout s;
    s.width  = vga_.Width();
    s.height = vga_.Height();
    s.stride = vga_.StrideBytes();
    s.start  = vga_.StartByte();
    s.bytes  = vga_.BytesPerPixel();
    const bool output = vga_.PanelPowered() || (standby_ & kStandbyDacPower);
    const uint64_t end = static_cast<uint64_t>(s.start) +
                         static_cast<uint64_t>(s.height - 1u) * s.stride +
                         static_cast<uint64_t>(s.width) * s.bytes;
    s.shown = output && !(standby_ & kStandbyClockStop) && s.bytes != 0u && s.stride != 0u &&
              end <= kVramBytes;
    return s;
}

void IteIt8181::PublishDisplay() {
    const Scanout s = Display();
    if (mode_latch_.Publish(emu_, s.shown, s.width, s.height)) {
        LOG(Lcd, "IteIt8181: display %ux%u %u bpp stride=%u start=0x%06X\n", s.width, s.height,
            s.bytes * 8u, s.stride, s.start);
    }
}

void IteIt8181::SaveState(StateWriter& w) {
    w.Write("cfg_mba", mba_);
    w.Write("cfg_gba", gba_);
    w.Write("cfg_sba", sba_);
    w.Write("cfg_test", test_);
    w.Write("cfg_standby", standby_);
    w.Write("cfg_pll1", pll1_);
    w.Write("cfg_pll2", pll2_);
    vga_.SaveState(w);
    gui_.SaveState(w);
    w.WriteBytes("vram", vram_.data(), vram_.size());
    mode_latch_.SaveState(w);
}

void IteIt8181::RestoreState(StateReader& r) {
    r.Read("cfg_mba", mba_);
    r.Read("cfg_gba", gba_);
    r.Read("cfg_sba", sba_);
    r.Read("cfg_test", test_);
    r.Read("cfg_standby", standby_);
    r.Read("cfg_pll1", pll1_);
    r.Read("cfg_pll2", pll2_);
    vga_.RestoreState(r);
    gui_.RestoreState(r);
    r.ReadBytes("vram", vram_.data(), vram_.size());
    mode_latch_.RestoreState(r);
}

}
