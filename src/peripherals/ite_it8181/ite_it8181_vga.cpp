#include "ite_it8181_vga.h"

#include "../vga/vga_index_set.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kAttrIndexData = 0x3C0;
constexpr uint32_t kAttrReadData  = 0x3C1;
constexpr uint32_t kMiscWrite     = 0x3C2;
constexpr uint32_t kSeqData       = 0x3C5;
constexpr uint32_t kGcData        = 0x3CF;
constexpr uint32_t kCrtcData      = 0x3D5;
constexpr uint32_t kEmaIndex      = 0x3D6;
constexpr uint32_t kEmaData       = 0x3D7;
constexpr uint32_t kInputStatus1  = 0x3DA;

constexpr uint8_t kPasswordIndex    = 0x0B;
constexpr uint8_t kLastStandardGr   = 0x08;
constexpr uint8_t kEmbEnable        = 0xCA;
constexpr uint8_t kEmbDisable       = 0x35;
constexpr uint8_t kEmaEnable        = 0xEC;
constexpr uint8_t kEmaDisable       = 0xCE;
constexpr uint8_t kExtensionEnabled = 0x01;

constexpr uint8_t kGrDisplayPage = 0x20;
constexpr uint8_t kGrDepth       = 0x38;
constexpr uint8_t kGrDepthField  = 0x07;
constexpr uint8_t kGrPageBits    = 0x0C;
constexpr uint32_t kPageBytes    = 0x00100000u;

constexpr uint8_t kEmaPanelPower    = 0x9B;
constexpr uint8_t kPanelPowerOn     = 0x01;
constexpr uint8_t kPanelPowerStatus = 0x08;

constexpr uint8_t kCrStartHi    = 0x0C;
constexpr uint8_t kCrStartLo    = 0x0D;
constexpr uint8_t kCrOffset     = 0x13;

constexpr uint32_t kStrideUnit = 16u;
constexpr uint32_t kStartUnit  = 8u;

constexpr uint64_t kClock25Hz = 25175000u;
constexpr uint64_t kClock28Hz = 28322000u;
constexpr uint8_t  kClockSelectField = 0x0C;
constexpr uint8_t  kClockSelect28    = 0x04;

constexpr VgaIndexSet kEmaWrites{{0x80, 0x93}, {0x9A, 0x9B}, {0xA0, 0xA3}, {0xA8, 0xA9}, {0xB0, 0xDF}};
constexpr VgaIndexSet kEmaReads{{0x80, 0x93}, {0x9B, 0x9B}, {0xA0, 0xA3}, {0xA8, 0xA9}, {0xB0, 0xDF}};
constexpr VgaIndexSet kGraphicsAccess{{0x06, 0x06}, {0x20, 0x20}, {0x24, 0x24}, {0x2B, 0x2B},
                                      {0x2F, 0x2F}, {0x38, 0x39}, {0x3F, 0x3F}, {0x50, 0x5F}};
constexpr VgaIndexSet kCrtcWrites{{0x00, 0x18}};
constexpr VgaIndexSet kCrtcReads{{0x00, 0x17}};
constexpr VgaIndexSet kSequencerAccess{{0x00, 0x00}};

bool AcceptedPage(uint8_t value) {
    return value == 0x01u || value == 0x05u || value == 0x09u;
}

uint32_t DepthBytes(uint8_t gr38) {
    switch (gr38 & kGrDepthField) {
        case 1u: return 1u;
        case 3u: return 2u;
        case 4u: return 3u;
        default: return 0u;
    }
}

}

void IteIt8181Vga::Attach() {
    retrace_.Attach();
}

void IteIt8181Vga::PowerOnReset() {
    regs_        = VgaRegisterFile{};
    for (uint8_t& b : ema_) b = 0u;
    ema_index_   = 0u;
    ema_enabled_ = false;
    emb_enabled_ = false;
}

uint8_t IteIt8181Vga::Read(uint32_t port) {
    switch (port) {
        case kGcData:       return ReadGraphicsData();
        case kEmaIndex:     return ema_index_;
        case kEmaData:      return ReadExtensionData();
        case kInputStatus1:
            regs_.ResetAttributeFlipFlop();
            return retrace_.InputStatus1(regs_, DotClock());
        case kSeqData:
            if (!kSequencerAccess.Has(regs_.SequencerIndex())) {
                emu_.Get<Fatal>().Die("IteIt8181Vga: read of SR%02X", regs_.SequencerIndex());
            }
            break;
        case kCrtcData:
            if (!kCrtcReads.Has(regs_.CrtcIndex())) {
                emu_.Get<Fatal>().Die("IteIt8181Vga: read of CR%02X", regs_.CrtcIndex());
            }
            break;
        case kAttrReadData:
            emu_.Get<Fatal>().Die("IteIt8181Vga: read of AR%02X", regs_.AttributeIndex());
        default: break;
    }
    uint8_t value = 0u;
    if (regs_.ReadPort(port, value)) return value;
    emu_.Get<Fatal>().Die("IteIt8181Vga: read of VGA port 0x%03X", port);
}

void IteIt8181Vga::Write(uint32_t port, uint8_t value) {
    switch (port) {
        case kGcData:   WriteGraphicsData(value); return;
        case kEmaIndex: ema_index_ = value; return;
        case kEmaData:  WriteExtensionData(value); return;
        case kSeqData:
            if (!kSequencerAccess.Has(regs_.SequencerIndex())) {
                emu_.Get<Fatal>().Die("IteIt8181Vga: SR%02X = 0x%02X", regs_.SequencerIndex(), value);
            }
            break;
        case kCrtcData:
            if (!kCrtcWrites.Has(regs_.CrtcIndex())) {
                emu_.Get<Fatal>().Die("IteIt8181Vga: CR%02X = 0x%02X", regs_.CrtcIndex(), value);
            }
            break;
        case kAttrIndexData:
            if (regs_.AttributeDataNext()) {
                emu_.Get<Fatal>().Die("IteIt8181Vga: AR%02X = 0x%02X", regs_.AttributeIndex(), value);
            }
            break;
        case kMiscWrite:
            if ((value & kClockSelectField) > kClockSelect28) {
                emu_.Get<Fatal>().Die("IteIt8181Vga: misc output 0x%02X selects external clock %u",
                                      value, (value & kClockSelectField) >> 2);
            }
            break;
        default: break;
    }
    if (regs_.WritePort(port, value)) return;
    emu_.Get<Fatal>().Die("IteIt8181Vga: write 0x%02X to VGA port 0x%03X", value, port);
}

uint8_t IteIt8181Vga::ReadGraphicsData() {
    const uint8_t index = regs_.GraphicsIndex();
    if (index == kPasswordIndex) return emb_enabled_ ? kExtensionEnabled : 0u;
    if (index > kLastStandardGr && !emb_enabled_) {
        emu_.Get<Fatal>().Die("IteIt8181Vga: GR 0x%02X read while extension mode B is locked",
                              index);
    }
    if (!kGraphicsAccess.Has(index)) emu_.Get<Fatal>().Die("IteIt8181Vga: read of GR%02X", index);
    return regs_.Graphics(index);
}

void IteIt8181Vga::WriteGraphicsData(uint8_t value) {
    const uint8_t index = regs_.GraphicsIndex();
    if (index == kPasswordIndex) {
        if (value == kEmbEnable)       emb_enabled_ = true;
        else if (value == kEmbDisable) emb_enabled_ = false;
        else emu_.Get<Fatal>().Die("IteIt8181Vga: extension mode B password 0x%02X", value);
        return;
    }
    if (index > kLastStandardGr && !emb_enabled_) {
        emu_.Get<Fatal>().Die("IteIt8181Vga: GR 0x%02X = 0x%02X while extension mode B is "
                              "locked", index, value);
    }
    if (!kGraphicsAccess.Has(index)) {
        emu_.Get<Fatal>().Die("IteIt8181Vga: GR%02X = 0x%02X", index, value);
    }
    if (index == kGrDisplayPage && !AcceptedPage(value)) {
        emu_.Get<Fatal>().Die("IteIt8181Vga: GR20 0x%02X is not a modeled display page", value);
    }
    if (index == kGrDepth && DepthBytes(value) == 0u) {
        emu_.Get<Fatal>().Die("IteIt8181Vga: GR38 0x%02X selects an unmodeled colour depth",
                              value);
    }
    regs_.WritePort(kGcData, value);
}

uint8_t IteIt8181Vga::ReadExtensionData() {
    if (ema_index_ == kPasswordIndex) return ema_enabled_ ? kExtensionEnabled : 0u;
    if (!ema_enabled_) {
        emu_.Get<Fatal>().Die("IteIt8181Vga: EMA 0x%02X read while extension mode A is locked",
                              ema_index_);
    }
    if (!kEmaReads.Has(ema_index_)) emu_.Get<Fatal>().Die("IteIt8181Vga: read of EMA%02X", ema_index_);
    if (ema_index_ == kEmaPanelPower) {
        const uint8_t on = ema_[kEmaPanelPower] & kPanelPowerOn;
        return static_cast<uint8_t>(on | (on ? kPanelPowerStatus : 0u));
    }
    return ema_[ema_index_];
}

void IteIt8181Vga::WriteExtensionData(uint8_t value) {
    if (ema_index_ == kPasswordIndex) {
        if (value == kEmaEnable)       ema_enabled_ = true;
        else if (value == kEmaDisable) ema_enabled_ = false;
        else emu_.Get<Fatal>().Die("IteIt8181Vga: extension mode A password 0x%02X", value);
        return;
    }
    if (!ema_enabled_) {
        emu_.Get<Fatal>().Die("IteIt8181Vga: EMA 0x%02X = 0x%02X while extension mode A is "
                              "locked", ema_index_, value);
    }
    if (!kEmaWrites.Has(ema_index_)) {
        emu_.Get<Fatal>().Die("IteIt8181Vga: EMA%02X = 0x%02X", ema_index_, value);
    }
    if (ema_index_ == kEmaPanelPower && (value & ~kPanelPowerOn)) {
        emu_.Get<Fatal>().Die("IteIt8181Vga: EMA 9B 0x%02X sets a panel power bit other than "
                              "the power-on request", value);
    }
    ema_[ema_index_] = value;
}

GuestCycleClock::Rate IteIt8181Vga::DotClock() const {
    const bool select28 = (regs_.Misc() & kClockSelectField) == kClockSelect28;
    return GuestCycleClock::Rate{select28 ? kClock28Hz : kClock25Hz, 1u};
}

bool IteIt8181Vga::PanelPowered() const {
    return (ema_[kEmaPanelPower] & kPanelPowerOn) != 0u;
}

uint32_t IteIt8181Vga::Width() const {
    return regs_.DisplayWidth();
}

uint32_t IteIt8181Vga::Height() const {
    return regs_.DisplayHeight();
}

uint32_t IteIt8181Vga::StrideBytes() const {
    return static_cast<uint32_t>(regs_.Crtc(kCrOffset)) * kStrideUnit;
}

uint32_t IteIt8181Vga::StartByte() const {
    const uint32_t start = (static_cast<uint32_t>(regs_.Crtc(kCrStartHi)) << 8) |
                           regs_.Crtc(kCrStartLo);
    const uint32_t page = (regs_.Graphics(kGrDisplayPage) & kGrPageBits) ? kPageBytes : 0u;
    return start * kStartUnit + page;
}

uint32_t IteIt8181Vga::BytesPerPixel() const {
    return DepthBytes(regs_.Graphics(kGrDepth));
}

uint32_t IteIt8181Vga::PaletteXrgb(uint8_t index) const {
    const uint8_t* e = regs_.PaletteEntry(index);
    return lcd_pixel::PackXrgb(lcd_pixel::Expand6(e[0]), lcd_pixel::Expand6(e[1]),
                               lcd_pixel::Expand6(e[2]));
}

void IteIt8181Vga::SaveState(StateWriter& w) const {
    regs_.SaveState(w);
    w.WriteBytes("ema", ema_, sizeof(ema_));
    w.Write("ema_index", ema_index_);
    w.Write<uint8_t>("ema_enabled", ema_enabled_ ? 1u : 0u);
    w.Write<uint8_t>("emb_enabled", emb_enabled_ ? 1u : 0u);
}

void IteIt8181Vga::RestoreState(StateReader& r) {
    regs_.RestoreState(r);
    r.ReadBytes("ema", ema_, sizeof(ema_));
    r.Read("ema_index", ema_index_);
    uint8_t b = 0u;
    r.Read("ema_enabled", b);
    ema_enabled_ = b != 0u;
    r.Read("emb_enabled", b);
    emb_enabled_ = b != 0u;
}

}
