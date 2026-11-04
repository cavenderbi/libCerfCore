#include "vga_standard_renderer.h"

#include "vga_register_file.h"
#include <cerfcore/lcd/lcd_pixel_expand.h>

#include <cstring>

namespace cerfcore {

namespace {

constexpr uint8_t kSrClockingMode  = 0x01;
constexpr uint8_t kSrDots8         = 0x01;
constexpr uint8_t kSrCharMapSelect = 0x03;

constexpr uint8_t kGrMode       = 0x05;
constexpr uint8_t kGrInterleave = 0x20;
constexpr uint8_t kGrShift256   = 0x40;
constexpr uint8_t kGrMisc       = 0x06;
constexpr uint8_t kGrGraphics   = 0x01;

constexpr uint8_t kCrHDispEnd    = 0x01;
constexpr uint8_t kCrOverflow    = 0x07;
constexpr uint8_t kCrPresetRow   = 0x08;
constexpr uint8_t kCrMaxScan     = 0x09;
constexpr uint8_t kCrCursorStart = 0x0A;
constexpr uint8_t kCrCursorEnd   = 0x0B;
constexpr uint8_t kCrCursorHi    = 0x0E;
constexpr uint8_t kCrCursorLo    = 0x0F;
constexpr uint8_t kCrOffset      = 0x13;
constexpr uint8_t kCrUnderline   = 0x14;
constexpr uint8_t kCrModeControl = 0x17;
constexpr uint8_t kCrLineCompare = 0x18;

constexpr uint8_t kCr09DoubleScan  = 0x80;
constexpr uint8_t kCr0ACursorOff   = 0x20;
constexpr uint8_t kCr14Div4        = 0x20;
constexpr uint8_t kCr14DoubleWord  = 0x40;
constexpr uint8_t kCr17Map13       = 0x01;
constexpr uint8_t kCr17Map14       = 0x02;
constexpr uint8_t kCr17ScanDiv2    = 0x04;
constexpr uint8_t kCr17Div2        = 0x08;
constexpr uint8_t kCr17AddressWrap = 0x20;
constexpr uint8_t kCr17ByteMode    = 0x40;
constexpr uint8_t kCr17SyncEnable  = 0x80;

constexpr uint8_t kArModeControl  = 0x10;
constexpr uint8_t kArLineGraphics = 0x04;
constexpr uint8_t kArBlink        = 0x08;
constexpr uint8_t kArPanCompare   = 0x20;
constexpr uint8_t kAr8Bit         = 0x40;
constexpr uint8_t kArP54Select    = 0x80;
constexpr uint8_t kArPlaneEnable  = 0x12;
constexpr uint8_t kArPanning      = 0x13;
constexpr uint8_t kArColorSelect  = 0x14;

constexpr uint32_t kFontRowBytes = 32u;
constexpr uint32_t kFontMapLow   = 0x4000u;
constexpr uint32_t kFontMapHigh  = 0x2000u;
constexpr uint32_t kFontPlane    = 2u;
constexpr uint32_t kPlanes       = 4u;

enum class Serializer { Text, Planar, Interleave, Color256 };

Serializer SerializerOf(const VgaRegisterFile& r) {
    if (!(r.Graphics(kGrMisc) & kGrGraphics)) return Serializer::Text;
    if (r.Graphics(kGrMode) & kGrShift256)    return Serializer::Color256;
    if (r.Graphics(kGrMode) & kGrInterleave)  return Serializer::Interleave;
    return Serializer::Planar;
}

uint32_t CharDots(const VgaRegisterFile& r) {
    if (SerializerOf(r) != Serializer::Text) return 8u;
    return (r.Sequencer(kSrClockingMode) & kSrDots8) ? 8u : 9u;
}

uint32_t FontMapBase(uint32_t map) {
    return (map & 3u) * kFontMapLow + (map >> 2) * kFontMapHigh;
}

class ScanOut {
public:
    ScanOut(const VgaRegisterFile& r, uint32_t start, const uint8_t* vram, uint32_t vram_bytes,
            uint64_t frame, uint32_t* dib, uint32_t w, uint32_t h);

    void Frame();

private:
    struct Planes {
        uint8_t p[kPlanes];
    };

    uint32_t Address(uint32_t counter, uint32_t row_scan) const;
    uint8_t  PlaneByte(uint32_t offset, uint32_t plane) const;
    Planes   Fetch(uint32_t offset) const;
    uint32_t Dac(uint8_t index) const;
    uint32_t Pen(uint8_t value) const { return pens_[value & plane_enable_]; }
    uint32_t PelShift() const;
    void     Scanline(uint32_t y, uint32_t counter, uint32_t row_scan, uint32_t shift);
    void     TextCell(uint32_t* line, int64_t x0, uint32_t counter, uint32_t row_scan) const;
    void     GraphicsCell(uint32_t* line, int64_t x0, const Planes& pl) const;
    void     Put(uint32_t* line, int64_t x, uint32_t xrgb) const {
        if (x >= 0 && x < static_cast<int64_t>(limit_)) line[x] = xrgb;
    }

    const VgaRegisterFile& r_;
    const uint8_t*         vram_;
    uint32_t               vram_bytes_;
    uint32_t*              dib_;
    uint32_t               w_;
    uint32_t               h_;
    Serializer             serializer_;
    uint32_t               char_dots_;
    uint32_t               columns_;
    uint32_t               limit_;
    uint32_t               lines_;
    uint32_t               start_;
    uint32_t               max_scan_;
    bool                   scan_double_;
    uint32_t               counter_shift_;
    uint32_t               counter_step_;
    uint32_t               row_advance_;
    uint32_t               line_compare_;
    uint8_t                plane_enable_;
    bool                   eight_bit_;
    uint32_t               pens_[16] = {};
    uint32_t               map_a_;
    uint32_t               map_b_;
    bool                   blink_enable_;
    bool                   blink_visible_;
    bool                   line_graphics_;
    uint32_t               underline_row_;
    bool                   cursor_visible_;
    uint32_t               cursor_at_;
    uint32_t               cursor_start_;
    uint32_t               cursor_end_;
};

ScanOut::ScanOut(const VgaRegisterFile& r, uint32_t start, const uint8_t* vram,
                 uint32_t vram_bytes, uint64_t frame, uint32_t* dib, uint32_t w, uint32_t h)
    : r_(r), vram_(vram), vram_bytes_(vram_bytes), dib_(dib), w_(w), h_(h) {
    const uint8_t mode = r.Attribute(kArModeControl);
    const uint8_t cr9  = r.Crtc(kCrMaxScan);
    const uint8_t cr14 = r.Crtc(kCrUnderline);
    const uint8_t cr17 = r.Crtc(kCrModeControl);
    const uint8_t ov   = r.Crtc(kCrOverflow);
    const uint8_t sr3  = r.Sequencer(kSrCharMapSelect);
    const uint8_t sel  = r.Attribute(kArColorSelect);

    serializer_    = SerializerOf(r);
    char_dots_     = CharDots(r);
    columns_       = static_cast<uint32_t>(r.Crtc(kCrHDispEnd)) + 1u;
    limit_         = columns_ * char_dots_ < w_ ? columns_ * char_dots_ : w_;
    lines_         = r.DisplayHeight();
    start_         = start;
    max_scan_      = cr9 & 0x1Fu;
    scan_double_   = (cr9 & kCr09DoubleScan) != 0u;
    counter_shift_ = (cr17 & kCr17ScanDiv2) ? 1u : 0u;
    counter_step_  = (cr14 & kCr14Div4) ? 4u : (cr17 & kCr17Div2) ? 2u : 1u;
    row_advance_   = static_cast<uint32_t>(r.Crtc(kCrOffset)) << 1;
    line_compare_  = r.Crtc(kCrLineCompare) | ((ov & 0x10u) << 4) | ((cr9 & 0x40u) << 3);
    plane_enable_  = r.Attribute(kArPlaneEnable) & 0x0Fu;
    eight_bit_     = (mode & kAr8Bit) != 0u;
    map_b_         = (sr3 & 0x03u) | ((sr3 >> 2) & 0x04u);
    map_a_         = ((sr3 >> 2) & 0x03u) | ((sr3 >> 3) & 0x04u);
    blink_enable_  = (mode & kArBlink) != 0u;
    blink_visible_ = ((frame >> 5) & 1u) == 0u;
    line_graphics_ = (mode & kArLineGraphics) != 0u;
    underline_row_ = cr14 & 0x1Fu;
    cursor_start_  = r.Crtc(kCrCursorStart) & 0x1Fu;
    cursor_end_    = r.Crtc(kCrCursorEnd) & 0x1Fu;
    cursor_at_     = (((static_cast<uint32_t>(r.Crtc(kCrCursorHi)) << 8) | r.Crtc(kCrCursorLo)) +
                      ((r.Crtc(kCrCursorEnd) >> 5) & 0x03u));
    cursor_visible_ = !(r.Crtc(kCrCursorStart) & kCr0ACursorOff) && cursor_end_ >= cursor_start_ &&
                      ((frame >> 4) & 1u) == 0u;

    for (uint32_t v = 0; v < 16u; ++v) {
        const uint8_t pal   = r.Attribute(static_cast<uint8_t>(v)) & 0x3Fu;
        uint8_t       index = (mode & kArP54Select)
                                  ? static_cast<uint8_t>((pal & 0x0Fu) | ((sel & 0x03u) << 4))
                                  : pal;
        index = static_cast<uint8_t>(index | ((sel & 0x0Cu) << 4));
        pens_[v] = Dac(index);
    }
}

uint32_t ScanOut::Address(uint32_t counter, uint32_t row_scan) const {
    const uint8_t cr17 = r_.Crtc(kCrModeControl);
    uint32_t      a    = counter;
    if (r_.Crtc(kCrUnderline) & kCr14DoubleWord) {
        a = counter << 2;
    } else if (!(cr17 & kCr17ByteMode)) {
        a = (counter << 1) | ((counter >> ((cr17 & kCr17AddressWrap) ? 15u : 13u)) & 1u);
    }
    if (!(cr17 & kCr17Map13)) a = (a & ~(1u << 13)) | ((row_scan & 1u) << 13);
    if (!(cr17 & kCr17Map14)) a = (a & ~(1u << 14)) | (((row_scan >> 1) & 1u) << 14);
    return a;
}

uint8_t ScanOut::PlaneByte(uint32_t offset, uint32_t plane) const {
    return vram_[(offset * kPlanes + plane) % vram_bytes_];
}

ScanOut::Planes ScanOut::Fetch(uint32_t offset) const {
    Planes pl;
    for (uint32_t p = 0; p < kPlanes; ++p) pl.p[p] = PlaneByte(offset, p);
    return pl;
}

uint32_t ScanOut::Dac(uint8_t index) const {
    const uint8_t* c = r_.PaletteEntry(index);
    return lcd_pixel::PackXrgb(lcd_pixel::Expand6(c[0]), lcd_pixel::Expand6(c[1]),
                               lcd_pixel::Expand6(c[2]));
}

uint32_t ScanOut::PelShift() const {
    const uint32_t v = r_.Attribute(kArPanning) & 0x0Fu;
    if (serializer_ == Serializer::Text && char_dots_ == 9u) return v >= 8u ? 0u : v + 1u;
    if (serializer_ == Serializer::Color256 && eight_bit_) return v & 0x06u;
    return v & 0x07u;
}

void ScanOut::Frame() {
    const uint32_t pel      = PelShift();
    const uint32_t byte_pan = (r_.Crtc(kCrPresetRow) >> 5) & 0x03u;
    const bool     pan_cut  = (r_.Attribute(kArModeControl) & kArPanCompare) != 0u;
    uint32_t       row_start = start_;
    uint32_t       row_scan  = r_.Crtc(kCrPresetRow) & 0x1Fu;
    bool           split     = false;
    for (uint32_t y = 0; y < lines_; ++y) {
        if (!split && (y >> counter_shift_) == line_compare_) {
            row_start = 0u;
            row_scan  = 0u;
            split     = true;
        }
        const bool panned = !(split && pan_cut);
        Scanline(y, row_start + (panned ? byte_pan : 0u), row_scan, panned ? pel : 0u);
        if (scan_double_ && !(y & 1u)) continue;
        if (row_scan >= max_scan_) {
            row_scan = 0u;
            row_start += row_advance_;
        } else {
            ++row_scan;
        }
    }
}

void ScanOut::Scanline(uint32_t y, uint32_t counter, uint32_t row_scan, uint32_t shift) {
    if (y >= h_) return;
    uint32_t* line = dib_ + static_cast<size_t>(y) * w_;
    uint32_t  step = 0u;
    for (uint32_t c = 0; c <= columns_; ++c) {
        const int64_t x0 = static_cast<int64_t>(c) * char_dots_ - static_cast<int64_t>(shift);
        if (serializer_ == Serializer::Text) {
            TextCell(line, x0, counter, row_scan);
        } else {
            GraphicsCell(line, x0, Fetch(Address(counter, row_scan)));
        }
        if (++step == counter_step_) {
            step = 0u;
            ++counter;
        }
    }
}

void ScanOut::TextCell(uint32_t* line, int64_t x0, uint32_t counter, uint32_t row_scan) const {
    const Planes  pl   = Fetch(Address(counter, row_scan));
    const uint8_t ch   = pl.p[0];
    const uint8_t attr = pl.p[1];
    const uint32_t map = (attr & 0x08u) ? map_a_ : map_b_;
    const uint8_t bits = PlaneByte(FontMapBase(map) + ch * kFontRowBytes + row_scan, kFontPlane);
    const uint8_t own  = attr & 0x0Fu;
    const uint8_t bg   = static_cast<uint8_t>((attr >> 4) & (blink_enable_ ? 0x07u : 0x0Fu));
    const uint8_t fg   = (blink_enable_ && (attr & 0x80u) && !blink_visible_) ? bg : own;
    const bool underline = (attr & 0x77u) == 0x01u && row_scan == underline_row_;
    const bool cursor = cursor_visible_ && counter == cursor_at_ && row_scan >= cursor_start_ &&
                        row_scan <= cursor_end_;
    const bool ninth  = line_graphics_ && (ch & 0xE0u) == 0xC0u;
    for (uint32_t i = 0; i < char_dots_; ++i) {
        const bool set = i < 8u ? ((bits >> (7u - i)) & 1u) != 0u : ninth && (bits & 1u);
        uint32_t   xrgb = Pen((set || underline) ? fg : bg);
        if (cursor) xrgb = Pen(own);
        Put(line, x0 + i, xrgb);
    }
}

void ScanOut::GraphicsCell(uint32_t* line, int64_t x0, const Planes& pl) const {
    if (serializer_ == Serializer::Color256) {
        for (uint32_t i = 0; i < 8u; ++i) {
            const uint8_t byte = pl.p[i >> 1];
            if (eight_bit_) {
                Put(line, x0 + i, Dac(byte));
                continue;
            }
            Put(line, x0 + i, Pen((i & 1u) ? (byte & 0x0Fu) : (byte >> 4)));
        }
        return;
    }
    for (uint32_t i = 0; i < 8u; ++i) {
        uint8_t v = 0u;
        if (serializer_ == Serializer::Interleave) {
            const uint32_t shift = 6u - 2u * (i & 3u);
            const uint8_t  lo    = (i < 4u) ? pl.p[0] : pl.p[1];
            const uint8_t  hi    = (i < 4u) ? pl.p[2] : pl.p[3];
            v = static_cast<uint8_t>(((lo >> shift) & 3u) | (((hi >> shift) & 3u) << 2));
        } else {
            const uint32_t bit = 7u - i;
            for (uint32_t p = 0; p < kPlanes; ++p) v |= static_cast<uint8_t>(((pl.p[p] >> bit) & 1u) << p);
        }
        Put(line, x0 + i, Pen(v));
    }
}

}

bool VgaStandardRenderer::DisplayEnabled(const VgaRegisterFile& regs) {
    return (regs.Crtc(kCrModeControl) & kCr17SyncEnable) != 0u;
}

uint32_t VgaStandardRenderer::Width(const VgaRegisterFile& regs) {
    return (static_cast<uint32_t>(regs.Crtc(kCrHDispEnd)) + 1u) * CharDots(regs);
}

uint32_t VgaStandardRenderer::Height(const VgaRegisterFile& regs) {
    return regs.DisplayHeight();
}

uint32_t VgaStandardRenderer::Depth(const VgaRegisterFile& regs) {
    switch (SerializerOf(regs)) {
        case Serializer::Color256:
            return (regs.Attribute(kArModeControl) & kAr8Bit) ? 8u : 4u;
        case Serializer::Interleave:
            return 2u;
        default:
            return 4u;
    }
}

void VgaStandardRenderer::Render(const VgaRegisterFile& regs, uint32_t start, const uint8_t* vram,
                                 uint32_t vram_bytes, uint64_t frame, uint32_t* dib,
                                 uint32_t dst_w, uint32_t dst_h) {
    std::memset(dib, 0, static_cast<size_t>(dst_w) * dst_h * 4u);
    if (!DisplayEnabled(regs)) return;
    ScanOut(regs, start, vram, vram_bytes, frame, dib, dst_w, dst_h).Frame();
}

}
