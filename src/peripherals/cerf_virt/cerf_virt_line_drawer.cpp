#include "cerf_virt_line_drawer.h"
#include "cerf_virt_blt_pixelops.h"
#include "../raster/gpe_line_walk.h"
#include <cerfcore/peripherals/cerf_virt/cerf_virt_framebuffer.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/log.h>

namespace cerfcore {

namespace CerfVirt {

REGISTER_SERVICE(CerfVirtLineDrawer);

bool CerfVirtLineDrawer::ShouldRegister() {
    return emu_.Get<DeviceConfig>().guest_additions;
}

void CerfVirtLineDrawer::OnReady() {
    fb_  = &emu_.Get<CerfVirtFramebuffer>();
    arena_  = &emu_.Get<CerfVirtDmaArena>();
    fatal_  = &emu_.Get<Fatal>();
}

bool CerfVirtLineDrawer::Execute(const CerfLineDescriptor& l) {
    if (l.magic != kCerfLineMagic) {
        fatal_->Die("[CerfVirtLineDrawer] corrupt line magic 0x%08X", l.magic);
    }
    if (l.i_dir < 0 || l.i_dir > 7) {
        fatal_->Die("[CerfVirtLineDrawer] corrupt iDir %d", l.i_dir);
    }
    const uint32_t bits = BltPixelOps::FormatBits(l.dst.format);
    if (bits == 0u) {
        fatal_->Die("[CerfVirtLineDrawer] UNIMPLEMENTED dst format %u (%u bpp)",
                    l.dst.format, bits);
    }
    const bool     subbyte = bits < 8u;
    const uint32_t bpp     = subbyte ? 1u : bits / 8u;
    Surface dst;
    if (!ResolveSurface(l.dst, bpp, &dst)) {
        fatal_->Die("[CerfVirtLineDrawer] dst surface unresolvable: buffer=0x%08X "
                    "is_fb_pa=%u fmt=%u stride=%d stage_off=0x%08X stage_len=%u",
                    l.dst.buffer, l.dst.is_fb_pa, l.dst.format, l.dst.stride,
                    l.dst.stage_off, l.dst.stage_len);
    }

    const uint32_t base_mask = (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    const uint32_t pen       = l.solid_color & base_mask;
    const uint8_t  rop2_mark  = (uint8_t)l.mix;
    const uint8_t  rop2_space = (uint8_t)(l.mix >> 8);

    GpeLineWalk walk(l.x_start, l.y_start, static_cast<uint32_t>(l.i_dir), l.d_m, l.d_n,
                     l.ll_gamma);
    int32_t style_state = l.style_state;
    Fatal& fatal = *fatal_;
    for (int32_t n = l.c_pels; n > 0; --n) {
        const int32_t x = walk.X();
        const int32_t y = walk.Y();
        const uint8_t rop2 = ((l.style >> ((uint32_t)(style_state++) & 31u)) & 1u)
                           ? rop2_space : rop2_mark;
        const bool clipped_out = l.has_clip != 0u &&
            (x < l.clip_rect.left || x >= l.clip_rect.right ||
             y < l.clip_rect.top  || y >= l.clip_rect.bottom);
        if (!clipped_out &&
            (l.band_y_count == 0u ||
             ((uint32_t)y >= l.band_y_first &&
              (uint32_t)y < l.band_y_first + l.band_y_count))) {
            if (subbyte) {
                uint32_t d = 0;
                if (!ReadSubBytePixel(dst, x, y, bits, &d)) {
                    fatal.Die("[CerfVirtLineDrawer] dst pixel unaddressable at x=%d y=%d: "
                                "buffer=0x%08X is_fb_pa=%u stage_off=0x%08X stage_len=%u",
                                x, y, l.dst.buffer, l.dst.is_fb_pa, l.dst.stage_off,
                                l.dst.stage_len);
                }
                const uint32_t v = BltPixelOps::ApplyRop2(rop2, pen, d) & base_mask;
                if (!WriteSubBytePixel(dst, x, y, bits, v)) {
                    fatal.Die("[CerfVirtLineDrawer] dst pixel unaddressable at x=%d y=%d: "
                                "buffer=0x%08X is_fb_pa=%u stage_off=0x%08X stage_len=%u",
                                x, y, l.dst.buffer, l.dst.is_fb_pa, l.dst.stage_off,
                                l.dst.stage_len);
                }
            } else {
                uint32_t run = 0;
                uint8_t* p = PixelPtr(dst, x, y, bpp, &run);
                if (!p) {
                    fatal.Die("[CerfVirtLineDrawer] dst pixel unaddressable at x=%d y=%d: "
                                "buffer=0x%08X is_fb_pa=%u stage_off=0x%08X stage_len=%u",
                                x, y, l.dst.buffer, l.dst.is_fb_pa, l.dst.stage_off,
                                l.dst.stage_len);
                }
                const uint32_t d = (run >= bpp) ? BltPixelOps::ReadPixel(p, bpp)
                                                : ReadStraddlePixel(dst, x, y, bpp);
                const uint32_t v = BltPixelOps::ApplyRop2(rop2, pen, d) & base_mask;
                if (run >= bpp) BltPixelOps::WritePixel(p, bpp, v);
                else            WriteStraddlePixel(dst, x, y, bpp, v);
            }
        }
        if (n == 1) break;
        walk.Advance();
    }
    fb_->MarkDirty();
    return true;
}

}

}
