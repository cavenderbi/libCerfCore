#include "ite_it8181_blitter.h"

#include "ite_it8181_vga.h"
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../raster/gpe_line_walk.h"
#include "../raster/rop3.h"

namespace cerfcore {

namespace {

constexpr uint32_t kHostSource   = 1u << 1;
constexpr uint32_t kDepthField   = 3u << 2;
constexpr uint32_t kMonoSource   = 1u << 4;
constexpr uint32_t kTransparent  = 1u << 5;
constexpr uint32_t kRopField     = 0xFFu << 8;
constexpr uint32_t kBackward     = 1u << 16;
constexpr uint32_t kSolidPattern = 1u << 17;
constexpr uint32_t kGo           = 1u << 24;
constexpr uint32_t kLine         = 1u << 25;
constexpr uint32_t kBlitBits     = kHostSource | kDepthField | kMonoSource | kTransparent |
                                   kRopField | kBackward | kSolidPattern | kGo;

constexpr uint32_t kLineCommand    = 0x022EF000u;
constexpr uint32_t kSolidLineStyle = 0x0000FFFFu;
constexpr uint8_t  kPatCopy        = 0xF0u;

constexpr uint32_t kPatternSide    = 8u;
constexpr uint32_t kPatternAddress = 0x00FFFFFFu;

constexpr uint32_t kCodeToDir[8] = { 4u, 7u, 5u, 6u, 3u, 0u, 2u, 1u };

uint32_t DepthOf(uint32_t bc) { return (bc & kDepthField) >> 2; }
uint8_t  RopOf(uint32_t bc)   { return static_cast<uint8_t>((bc & kRopField) >> 8); }
uint32_t WidthOf(uint32_t bwh)  { return bwh >> 16; }
uint32_t HeightOf(uint32_t bwh) { return bwh & 0xFFFFu; }
int64_t  XOf(uint32_t xy) { return static_cast<int64_t>(xy & 0xFFFFu); }
int64_t  YOf(uint32_t xy) { return static_cast<int64_t>(xy >> 16); }

int64_t Signed14(uint32_t v) {
    v &= 0x3FFFu;
    return (v & 0x2000u) ? static_cast<int64_t>(v) - 0x4000 : static_cast<int64_t>(v);
}

}

void IteIt8181Blitter::CheckCommand(const Registers& r) const {
    const uint32_t bc = r.bc;
    Fatal& fatal = emu_.Get<Fatal>();
    if (DepthOf(bc) == 3u) fatal.Die("IteIt8181Blitter: BC 0x%08X selects depth field 3", bc);
    if (bc & kLine) {
        if ((bc & ~kDepthField) != kLineCommand) {
            fatal.Die("IteIt8181Blitter: line command BC 0x%08X is not modeled", bc);
        }
        if (r.lsr != kSolidLineStyle) {
            fatal.Die("IteIt8181Blitter: line with style LSR 0x%08X is not modeled", r.lsr);
        }
        return;
    }
    if (!(bc & kGo)) fatal.Die("IteIt8181Blitter: BC 0x%08X without the start bit", bc);
    if (bc & ~kBlitBits) fatal.Die("IteIt8181Blitter: BC 0x%08X sets unmodeled bits", bc);
    if ((bc & kMonoSource) && !(bc & kHostSource)) {
        fatal.Die("IteIt8181Blitter: BC 0x%08X expands a mono screen source", bc);
    }
    if ((bc & kTransparent) && !(bc & kMonoSource)) {
        fatal.Die("IteIt8181Blitter: BC 0x%08X makes a colour source transparent", bc);
    }
    if ((bc & kHostSource) && (bc & kBackward)) {
        fatal.Die("IteIt8181Blitter: BC 0x%08X runs a host source backward", bc);
    }
}

size_t IteIt8181Blitter::HostBytes(const Registers& r) const {
    if ((r.bc & kLine) || !(r.bc & kHostSource)) return 0u;
    const size_t w = WidthOf(r.bwh);
    const size_t h = HeightOf(r.bwh);
    if (r.bc & kMonoSource) return (h * ((w + 7u) / 8u) + 3u) & ~static_cast<size_t>(3u);
    const size_t bytes = DepthOf(r.bc) + 1u;
    return h * (((w * bytes) + 3u) & ~static_cast<size_t>(3u));
}

void IteIt8181Blitter::Execute(const Registers& r, const uint8_t* host) {
    if (r.bc & kLine) Line(r);
    else              Rectangle(r, host);
}

IteIt8181Blitter::Target IteIt8181Blitter::TargetOf(const Registers& r) const {
    Target t;
    t.bytes = DepthOf(r.bc) + 1u;
    t.mask  = static_cast<uint32_t>(cerf::ByteWidthMask(t.bytes));
    t.pitch = r.bso;
    return t;
}

uint32_t IteIt8181Blitter::PixelAddress(const Target& t, int64_t x, int64_t y) const {
    const uint64_t addr = (static_cast<uint64_t>(y) * t.pitch + static_cast<uint64_t>(x)) * t.bytes;
    if (x < 0 || y < 0 || addr + t.bytes > vram_.size()) {
        emu_.Get<Fatal>().Die("IteIt8181Blitter: pixel (%lld,%lld) at pitch %u is outside the "
                              "frame buffer", static_cast<long long>(x),
                              static_cast<long long>(y), t.pitch);
    }
    return static_cast<uint32_t>(addr);
}

uint32_t IteIt8181Blitter::ReadPixel(const Target& t, int64_t x, int64_t y) const {
    return static_cast<uint32_t>(cerf::le::UN(vram_.data() + PixelAddress(t, x, y), t.bytes));
}

void IteIt8181Blitter::WritePixel(const Target& t, int64_t x, int64_t y, uint32_t value) {
    cerf::le::PutN(vram_.data() + PixelAddress(t, x, y), value & t.mask, t.bytes);
}

uint32_t IteIt8181Blitter::PatternPixel(const Registers& r, const Target& t, int64_t x,
                                        int64_t y) const {
    const int64_t  ox  = static_cast<int64_t>(r.bpoa >> 28);
    const int64_t  oy  = static_cast<int64_t>((r.bpoa >> 24) & 0xFu);
    const uint64_t px  = static_cast<uint64_t>(x - ox) & (kPatternSide - 1u);
    const uint64_t py  = static_cast<uint64_t>(y - oy) & (kPatternSide - 1u);
    const uint64_t at  = static_cast<uint64_t>(vga_.StartByte()) + (r.bpoa & kPatternAddress) +
                         (py * kPatternSide + px) * t.bytes;
    if (at + t.bytes > vram_.size()) {
        emu_.Get<Fatal>().Die("IteIt8181Blitter: pattern at 0x%08X is outside the frame buffer",
                              r.bpoa);
    }
    return static_cast<uint32_t>(cerf::le::UN(vram_.data() + at, t.bytes));
}

void IteIt8181Blitter::Rectangle(const Registers& r, const uint8_t* host) {
    const Target   t      = TargetOf(r);
    const uint8_t  rop    = RopOf(r.bc);
    const bool     back   = (r.bc & kBackward) != 0u;
    const bool     solid  = (r.bc & kSolidPattern) != 0u;
    const bool     hosted = (r.bc & kHostSource) != 0u;
    const bool     mono   = (r.bc & kMonoSource) != 0u;
    const bool     clear  = (r.bc & kTransparent) != 0u;
    const bool     need_p = Rop3ReadsPattern(rop);
    const bool     need_s = Rop3ReadsSource(rop);
    const uint32_t w      = WidthOf(r.bwh);
    const uint32_t h      = HeightOf(r.bwh);
    const int64_t  step   = back ? -1 : 1;
    const size_t   mono_row  = (w + 7u) / 8u;
    const size_t   color_row = ((static_cast<size_t>(w) * t.bytes) + 3u) & ~static_cast<size_t>(3u);
    for (uint32_t row = 0; row < h; ++row) {
        const int64_t dy = YOf(r.bdls) + step * row;
        const int64_t sy = YOf(r.bsle) + step * row;
        for (uint32_t col = 0; col < w; ++col) {
            const int64_t dx = XOf(r.bdls) + step * col;
            uint32_t s = 0u;
            if (hosted && mono) {
                const uint8_t byte = host[row * mono_row + col / 8u];
                const bool    set  = ((byte >> (7u - (col & 7u))) & 1u) != 0u;
                if (!set && clear) continue;
                s = set ? r.fcr : r.bcr;
            } else if (hosted) {
                s = static_cast<uint32_t>(
                    cerf::le::UN(host + row * color_row + static_cast<size_t>(col) * t.bytes, t.bytes));
            } else if (need_s) {
                s = ReadPixel(t, XOf(r.bsle) + step * col, sy);
            }
            const uint32_t p = !need_p ? 0u : solid ? r.fcr : PatternPixel(r, t, dx, dy);
            WritePixel(t, dx, dy, Rop3(rop, p, s, ReadPixel(t, dx, dy)));
        }
    }
}

void IteIt8181Blitter::Line(const Registers& r) {
    const Target   t      = TargetOf(r);
    const uint32_t code   = r.let >> 28;
    const uint32_t count  = (r.let >> 16) & 0xFFFu;
    const int64_t  error  = Signed14(r.let);
    const int64_t  axial  = static_cast<int64_t>(r.asds >> 16);
    const int64_t  diag   = Signed14(r.asds);
    if ((axial & 1) || (diag & 1)) {
        emu_.Get<Fatal>().Die("IteIt8181Blitter: line steps ASDS 0x%08X are not doubled",
                              r.asds);
    }
    const int64_t d_n   = axial / 2;
    const int64_t d_m   = d_n - diag / 2;
    const int64_t gamma = error - d_n + ((code & 1u) ? 0 : 1);
    const int64_t left = XOf(r.lst), top = YOf(r.lst);
    const int64_t right = XOf(r.lsb), bottom = YOf(r.lsb);
    GpeLineWalk walk(static_cast<int32_t>(XOf(r.bdls)), static_cast<int32_t>(YOf(r.bdls)),
                     kCodeToDir[code & 7u], d_m, d_n, gamma);
    for (uint32_t n = 0; n < count; ++n) {
        const int64_t x = walk.X();
        const int64_t y = walk.Y();
        if (x >= left && x <= right && y >= top && y <= bottom) {
            WritePixel(t, x, y, Rop3(kPatCopy, r.fcr, 0u, ReadPixel(t, x, y)));
        }
        walk.Advance();
    }
}

}
