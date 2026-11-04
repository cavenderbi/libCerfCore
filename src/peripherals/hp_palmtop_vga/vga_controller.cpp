#include "vga_controller.h"

#include "../vga/vga_index_set.h"
#include "../vga/vga_standard_renderer.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>
#include <cerfcore/state/state_stream.h>

#include <algorithm>
#include <cstring>

namespace cerfcore {

namespace {

constexpr uint32_t kAttrIndexData   = 0x3C0;
constexpr uint32_t kAttrReadData    = 0x3C1;
constexpr uint32_t kMiscWrite       = 0x3C2;
constexpr uint32_t kVgaEnable       = 0x3C3;
constexpr uint32_t kSeqData         = 0x3C5;
constexpr uint32_t kPelMask         = 0x3C6;
constexpr uint32_t kDacReadIndex    = 0x3C7;
constexpr uint32_t kDacWriteIndex   = 0x3C8;
constexpr uint32_t kDacData         = 0x3C9;
constexpr uint32_t kGraphics2Pos    = 0x3CA;
constexpr uint32_t kGraphics1Pos    = 0x3CC;
constexpr uint32_t kGcData          = 0x3CF;
constexpr uint32_t kCrtcData        = 0x3D5;
constexpr uint32_t kBankWrite       = 0x3D8;
constexpr uint32_t kBankRead        = 0x3D9;
constexpr uint32_t kInputStatus1    = 0x3DA;
constexpr uint32_t kClockControl    = 0x3DB;
constexpr uint32_t kMemClockLo      = 0x43C6;
constexpr uint32_t kMemClockHi      = 0x43C7;
constexpr uint32_t kVidClockLo      = 0x43C8;
constexpr uint32_t kVidClockHi      = 0x43C9;
constexpr uint32_t kSubsystemEnable = 0x46E8;

constexpr uint8_t kVgaEnabled       = 0x01;
constexpr uint8_t kSubsystemEnabled = 0x08;

constexpr uint8_t kSrClockingMode = 0x01;
constexpr uint8_t kSrShiftLoad    = 0x04;
constexpr uint8_t kSrShift4       = 0x10;
constexpr uint8_t kSrScreenOff    = 0x20;

constexpr uint8_t kArPanning    = 0x13;
constexpr uint8_t kArPanningMax = 0x08;
constexpr uint8_t kSrChipVersion  = 0x0B;
constexpr uint8_t kSrPowerUpMode1 = 0x0C;
constexpr uint8_t kSrModeControl2 = 0x0D;
constexpr uint8_t kSrModeControl1 = 0x0E;

constexpr uint8_t kChipVersion       = 0xE3;
constexpr uint8_t kNewMode1BankFlip  = 0x02;
constexpr uint8_t kNewMode1Unlock    = 0x80;
constexpr uint8_t kOldMode2Emulation = 0x07;
constexpr uint8_t kOldMode2Paging    = 0x10;

constexpr uint8_t kGrMiscExtFunc = 0x0F;
constexpr uint8_t kGrDualBanks   = 0x01;
constexpr uint8_t kGrAltBanks    = 0x04;
constexpr uint8_t kGrWideChars   = 0x08;
constexpr uint8_t kBankMask      = 0x1F;

constexpr uint8_t kClockHalf          = 0x20;
constexpr uint8_t kVidClockUnusedBits = 0xE0;

constexpr uint8_t kCrStartHi     = 0x0C;
constexpr uint8_t kCrStartLo     = 0x0D;
constexpr uint8_t kCrOffset      = 0x13;
constexpr uint8_t kCrModuleTest  = 0x1E;
constexpr uint8_t kCrInterlace   = 0x04;
constexpr uint8_t kCrStartBit16  = 0x20;
constexpr uint8_t kCrHostBit16   = 0x80;
constexpr uint8_t kCrLinearAddr  = 0x21;
constexpr uint8_t kCrStartHigh   = 0x27;
constexpr uint8_t kCrPixelBus    = 0x38;

constexpr uint8_t kLinearEnable = 0x20;
constexpr uint8_t kLinear2Mb    = 0x10;

constexpr uint8_t kPixelDepthShift = 2;
constexpr uint8_t kPixelDepthField = 0x0C;
constexpr uint8_t kPixelDepthHiColor = 1;
constexpr uint8_t kPixelBus8Gate   = 0x10;

constexpr uint8_t kDacModeField = 0xF0;
// constexpr uint8_t kDacMode15    = 0x10;
constexpr uint8_t kDacMode16    = 0x30;
constexpr uint8_t kDacUnlockReads = 4;

constexpr uint64_t kClock25Hz       = 25175000u;
constexpr uint64_t kClock28Hz       = 28322000u;
constexpr uint64_t kVidClockRefHz   = 14318180u;
constexpr uint8_t  kClockSelectField = 0x0C;
constexpr uint8_t  kClockSelectVclk  = 0x08;

constexpr VgaIndexSet kSequencerWrites{{0x00, 0x04}, {0x0B, 0x0F}};
constexpr VgaIndexSet kSequencerReads{{0x02, 0x02}};
constexpr VgaIndexSet kCrtcWrites{{0x00, 0x19}, {0x1E, 0x21}, {0x23, 0x23}, {0x25, 0x25},
                                  {0x27, 0x27}, {0x29, 0x2A}, {0x2F, 0x30}, {0x33, 0x33},
                                  {0x36, 0x39}, {0x50, 0x50}};
constexpr VgaIndexSet kGraphicsWrites{{0x00, 0x08}, {0x0F, 0x0F}, {0x2F, 0x2F}};
constexpr VgaIndexSet kAttributeWrites{{0x00, 0x14}};

}

VgaController::VgaController(CerfEmulator& emu)
    : emu_(emu), retrace_(emu), fb_(kVramBytes, 0u) {}

void VgaController::Attach() {
    retrace_.Attach();
}

void VgaController::PowerOnReset() {
    regs_        = VgaRegisterFile{};
    new_mode_    = false;
    sr0d_old_    = 0u;
    sr0d_new_    = 0u;
    sr0e_old_    = 0u;
    sr0e_new_    = 0u;
    bank_write_  = 0u;
    bank_read_   = 0u;
    port_3db_    = 0u;
    mem_clock_[0] = mem_clock_[1] = 0u;
    vid_clock_[0] = vid_clock_[1] = 0u;
    dac_command_ = 0u;
    dac_reads_   = 0u;
}

uint32_t VgaController::Width() const {
    return regs_.DisplayWidth();
}

uint32_t VgaController::Height() const {
    return regs_.DisplayHeight();
}

uint32_t VgaController::StrideBytes() const {
    return static_cast<uint32_t>(regs_.Crtc(kCrOffset)) << 3;
}

uint32_t VgaController::StartAddress() const {
    uint32_t start = (static_cast<uint32_t>(regs_.Crtc(kCrStartHi)) << 8) | regs_.Crtc(kCrStartLo);
    const uint8_t module_test = regs_.Crtc(kCrModuleTest);
    if (module_test & kCrHostBit16) start |= static_cast<uint32_t>(module_test & kCrStartBit16) << 11;
    start |= static_cast<uint32_t>(regs_.Crtc(kCrStartHigh) & 0x03u) << 17;
    return start;
}

bool VgaController::PackedPixels() const {
    const uint8_t bus = regs_.Crtc(kCrPixelBus);
    if (((bus & kPixelDepthField) >> kPixelDepthShift) == kPixelDepthHiColor) return true;
    return !(bus & kPixelBus8Gate) || (regs_.Crtc(kCrModuleTest) & kCrHostBit16);
}

uint32_t VgaController::Bpp() const {
    const uint8_t bus = regs_.Crtc(kCrPixelBus);
    if (((bus & kPixelDepthField) >> kPixelDepthShift) != kPixelDepthHiColor) return 8u;
    return (dac_command_ & kDacModeField) == kDacMode16 ? 16u : 15u;
}

bool VgaController::CurrentMode(uint32_t& w, uint32_t& h, uint32_t& bpp) const {
    if (!VgaStandardRenderer::DisplayEnabled(regs_)) return false;
    if (!PackedPixels()) {
        w   = VgaStandardRenderer::Width(regs_);
        h   = VgaStandardRenderer::Height(regs_);
        bpp = VgaStandardRenderer::Depth(regs_);
        return true;
    }
    w = Width(); h = Height(); bpp = Bpp();
    return w > 0 && h > 0 && StrideBytes() > 0;
}

bool VgaController::HasFrame() {
    uint32_t w = 0, h = 0, bpp = 0;
    return CurrentMode(w, h, bpp) && !(regs_.Sequencer(kSrClockingMode) & kSrScreenOff);
}

bool VgaController::LinearOffset(uint32_t card_offset, uint32_t& vram_offset) const {
    const uint8_t cr21 = regs_.Crtc(kCrLinearAddr);
    if (!(cr21 & kLinearEnable)) return false;
    const uint32_t base = (static_cast<uint32_t>(cr21 & 0x0Fu) << 20) |
                          (static_cast<uint32_t>(cr21 >> 6) << 24);
    const uint32_t size = (cr21 & kLinear2Mb) ? 0x200000u : 0x100000u;
    if (card_offset - base >= size) return false;
    vram_offset = (card_offset - base) % kVramBytes;
    return true;
}

uint8_t VgaController::ReadReg8(uint32_t port) {
    switch (port) {
        case kSeqData: return ReadSequencerData();
        case kPelMask: return ReadPelMask();
        case kAttrReadData: case kGcData: case kCrtcData:
            emu_.Get<Fatal>().Die("VgaController: read of data port 0x%03X", port);
        case kDacReadIndex: case kDacWriteIndex: case kDacData:
            dac_reads_ = 0u;
            break;
        case kBankWrite: case kBankRead: return ReadBank(port);
        case kInputStatus1:
            regs_.ResetAttributeFlipFlop();
            return retrace_.InputStatus1(regs_, DotClock());
        case kClockControl: return port_3db_;
        case kMemClockLo:   return mem_clock_[0];
        case kMemClockHi:   return mem_clock_[1];
        case kVidClockLo:   return vid_clock_[0];
        case kVidClockHi:   return vid_clock_[1];
        default: break;
    }
    uint8_t value = 0u;
    if (regs_.ReadPort(port, value)) return value;
    emu_.Get<Fatal>().Die("VgaController: read of port 0x%04X", port);
}

void VgaController::WriteReg8(uint32_t port, uint8_t value) {
    switch (port) {
        case kVgaEnable:
            if (value == kVgaEnabled) return;
            break;
        case kSubsystemEnable:
            if (value == kSubsystemEnabled) return;
            break;
        case kSeqData:  WriteSequencerData(value); return;
        case kPelMask:  WritePelMask(value); return;
        case kCrtcData: WriteCrtcData(value); return;
        case kMiscWrite:
            if ((value & kClockSelectField) > kClockSelectVclk) break;
            regs_.WritePort(port, value);
            return;
        case kGcData:
            if (!kGraphicsWrites.Has(regs_.GraphicsIndex())) break;
            regs_.WritePort(port, value);
            return;
        case kAttrIndexData:
            if (regs_.AttributeDataNext() && !kAttributeWrites.Has(regs_.AttributeIndex())) break;
            if (regs_.AttributeDataNext() && regs_.AttributeIndex() == kArPanning &&
                value > kArPanningMax) {
                emu_.Get<Fatal>().Die("VgaController: AR13 pel panning 0x%02X", value);
            }
            regs_.WritePort(port, value);
            return;
        case kDacReadIndex: case kDacWriteIndex: case kDacData:
            dac_reads_ = 0u;
            regs_.WritePort(port, value);
            return;
        case kGraphics2Pos: case kGraphics1Pos:
            return;
        case kBankWrite: case kBankRead: WriteBank(port, value); return;
        case kClockControl:
            if (value & ~kClockHalf) break;
            port_3db_ = value;
            return;
        case kVidClockHi:
            if (!ClockWritesUnlocked()) return;
            if (value & kVidClockUnusedBits) break;
            vid_clock_[1] = value;
            return;
        case kVidClockLo:
            if (ClockWritesUnlocked()) vid_clock_[0] = value;
            return;
        case kMemClockLo:
            if (ClockWritesUnlocked()) mem_clock_[0] = value;
            return;
        case kMemClockHi:
            if (ClockWritesUnlocked()) mem_clock_[1] = value;
            return;
        default:
            if (regs_.WritePort(port, value)) return;
            break;
    }
    emu_.Get<Fatal>().Die("VgaController: write 0x%02X to port 0x%04X", value, port);
}

uint8_t VgaController::ReadSequencerData() {
    switch (regs_.SequencerIndex()) {
        case kSrChipVersion:
            new_mode_ = true;
            return kChipVersion;
        default: break;
    }
    if (!kSequencerReads.Has(regs_.SequencerIndex())) {
        emu_.Get<Fatal>().Die("VgaController: read of SR%02X", regs_.SequencerIndex());
    }
    uint8_t value = 0u;
    regs_.ReadPort(kSeqData, value);
    return value;
}

void VgaController::WriteSequencerData(uint8_t value) {
    if (!kSequencerWrites.Has(regs_.SequencerIndex())) {
        emu_.Get<Fatal>().Die("VgaController: SR%02X = 0x%02X", regs_.SequencerIndex(), value);
    }
    switch (regs_.SequencerIndex()) {
        case kSrChipVersion:
            new_mode_ = false;
            return;
        case kSrModeControl2:
            if (new_mode_) {
                if (value != 0u) {
                    emu_.Get<Fatal>().Die("VgaController: new mode control 2 0x%02X selects an "
                                          "unmodeled clock", value);
                }
                sr0d_new_ = value;
            } else {
                if (value & (kOldMode2Emulation | kOldMode2Paging)) {
                    emu_.Get<Fatal>().Die("VgaController: old mode control 2 0x%02X selects an "
                                          "unmodeled emulation or paging mode", value);
                }
                sr0d_old_ = value;
            }
            return;
        case kSrModeControl1:
            if (new_mode_) sr0e_new_ = static_cast<uint8_t>(value ^ kNewMode1BankFlip);
            else           sr0e_old_ = value;
            return;
        case kSrPowerUpMode1:
            if (!(sr0e_new_ & kNewMode1Unlock)) return;
            break;
        case kSrClockingMode:
            if (value & (kSrShiftLoad | kSrShift4)) {
                emu_.Get<Fatal>().Die("VgaController: SR01 0x%02X loads the serializers every "
                                      "2nd or 4th character clock", value);
            }
            break;
        default: break;
    }
    regs_.WritePort(kSeqData, value);
}

void VgaController::WriteCrtcData(uint8_t value) {
    const uint8_t index = regs_.CrtcIndex();
    if (!kCrtcWrites.Has(index)) {
        emu_.Get<Fatal>().Die("VgaController: CR%02X = 0x%02X", index, value);
    }
    if (index == kCrPixelBus &&
        ((value & kPixelDepthField) >> kPixelDepthShift) > kPixelDepthHiColor) {
        emu_.Get<Fatal>().Die("VgaController: CR38 0x%02X selects an unmodeled pixel depth", value);
    }
    if (index == kCrModuleTest && (value & kCrInterlace)) {
        emu_.Get<Fatal>().Die("VgaController: CR1E 0x%02X selects interlaced scan-out", value);
    }
    regs_.WritePort(kCrtcData, value);
}

uint8_t VgaController::ReadPelMask() {
    if (dac_reads_ >= kDacUnlockReads) return dac_command_;
    ++dac_reads_;
    uint8_t value = 0u;
    regs_.ReadPort(kPelMask, value);
    return value;
}

void VgaController::WritePelMask(uint8_t value) {
    if (dac_reads_ >= kDacUnlockReads) {
        dac_command_ = value;
        dac_reads_   = 0u;
        return;
    }
    regs_.WritePort(kPelMask, value);
}

void VgaController::WriteBank(uint32_t port, uint8_t value) {
    const uint8_t gr0f = regs_.Graphics(kGrMiscExtFunc);
    if (!(gr0f & kGrAltBanks)) {
        emu_.Get<Fatal>().Die("VgaController: bank 0x%02X to port 0x%03X while GR0F 0x%02X "
                              "disables the alternate bank registers", value, port, gr0f);
    }
    const uint8_t bank = value & kBankMask;
    if (port == kBankWrite) {
        bank_write_ = bank;
        if (!(gr0f & kGrDualBanks)) bank_read_ = bank;
        return;
    }
    if (gr0f & kGrDualBanks) bank_read_ = bank;
}

uint8_t VgaController::ReadBank(uint32_t port) const {
    const uint8_t gr0f = regs_.Graphics(kGrMiscExtFunc);
    if (!(gr0f & kGrAltBanks)) {
        emu_.Get<Fatal>().Die("VgaController: read of port 0x%03X while GR0F 0x%02X disables "
                              "the alternate bank registers", port, gr0f);
    }
    return port == kBankWrite ? bank_write_ : bank_read_;
}

bool VgaController::ClockWritesUnlocked() const {
    return (sr0e_new_ & (kNewMode1BankFlip | kNewMode1Unlock)) == kNewMode1Unlock;
}

GuestCycleClock::Rate VgaController::DotClock() const {
    const uint8_t  misc   = regs_.Misc();
    const uint32_t select = (misc >> 2) & 3u;
    GuestCycleClock::Rate rate{kClock25Hz, 1u};
    if (select == 1u) rate = GuestCycleClock::Rate{kClock28Hz, 1u};
    if (select == 2u) {
        const uint32_t word = vid_clock_[0] | (static_cast<uint32_t>(vid_clock_[1]) << 8);
        const uint64_t n    = word & 0x7Fu;
        const uint64_t m    = (word >> 7) & 0x1Fu;
        const uint64_t k    = (word >> 12) & 0x01u;
        rate = GuestCycleClock::Rate{(n + 8u) * kVidClockRefHz, (m + 2u) << k};
    }
    if (regs_.Graphics(kGrMiscExtFunc) & kGrWideChars) rate.den *= 2u;
    if (port_3db_ & kClockHalf) rate.den *= 2u;
    return rate;
}

void VgaController::SaveState(StateWriter& w) const {
    regs_.SaveState(w);
    w.Write<uint8_t>("trident_new_mode", new_mode_ ? 1u : 0u);
    w.Write("sr0d_old", sr0d_old_);
    w.Write("sr0d_new", sr0d_new_);
    w.Write("sr0e_old", sr0e_old_);
    w.Write("sr0e_new", sr0e_new_);
    w.Write("bank_write", bank_write_);
    w.Write("bank_read", bank_read_);
    w.Write("port_3db", port_3db_);
    w.Write("mem_clock", mem_clock_);
    w.Write("vid_clock", vid_clock_);
    w.Write("dac_command", dac_command_);
    w.Write("dac_reads", dac_reads_);
    w.WriteBytes("fb", fb_.data(), fb_.size());
}

void VgaController::RestoreState(StateReader& r) {
    regs_.RestoreState(r);
    uint8_t new_mode = 0u;
    r.Read("trident_new_mode", new_mode);
    new_mode_ = new_mode != 0u;
    r.Read("sr0d_old", sr0d_old_);
    r.Read("sr0d_new", sr0d_new_);
    r.Read("sr0e_old", sr0e_old_);
    r.Read("sr0e_new", sr0e_new_);
    r.Read("bank_write", bank_write_);
    r.Read("bank_read", bank_read_);
    r.Read("port_3db", port_3db_);
    r.Read("mem_clock", mem_clock_);
    r.Read("vid_clock", vid_clock_);
    r.Read("dac_command", dac_command_);
    r.Read("dac_reads", dac_reads_);
    r.ReadBytes("fb", fb_.data(), fb_.size());
}

void VgaController::RenderInto(uint32_t* dib, uint32_t dst_w, uint32_t dst_h) {
    if (!PackedPixels()) {
        VgaStandardRenderer::Render(regs_, StartAddress(), fb_.data(), kVramBytes,
                                    VgaRetrace::HostFrameCount(regs_, DotClock()), dib, dst_w,
                                    dst_h);
        return;
    }
    std::memset(dib, 0, static_cast<size_t>(dst_w) * dst_h * 4u);

    uint32_t gw = 0, gh = 0, bpp = 0;
    if (!CurrentMode(gw, gh, bpp)) return;
    const uint64_t stride = StrideBytes();
    const uint64_t start  = static_cast<uint64_t>(StartAddress()) * 4u;

    const uint8_t* fb = fb_.data();
    const uint32_t cw = std::min(dst_w, gw);
    const uint32_t ch = std::min(dst_h, gh);

    for (uint32_t y = 0; y < ch; ++y) {
        uint32_t* row = dib + static_cast<size_t>(y) * dst_w;
        uint32_t  a   = static_cast<uint32_t>((start + y * stride) % kVramBytes);
        const auto next = [&a, fb]() {
            const uint8_t b = fb[a];
            if (++a == kVramBytes) a = 0u;
            return b;
        };
        if (bpp == 8u) {
            for (uint32_t x = 0; x < cw; ++x) {
                const uint8_t* c = regs_.PaletteEntry(next());
                row[x] = lcd_pixel::PackXrgb(lcd_pixel::Expand6(c[0]), lcd_pixel::Expand6(c[1]),
                                             lcd_pixel::Expand6(c[2]));
            }
            continue;
        }
        for (uint32_t x = 0; x < cw; ++x) {
            const uint8_t  lo = next();
            const uint16_t px = static_cast<uint16_t>(lo | (next() << 8));
            row[x] = bpp == 16u ? lcd_pixel::Expand565(px) : lcd_pixel::Expand555(px);
        }
    }
}

}
