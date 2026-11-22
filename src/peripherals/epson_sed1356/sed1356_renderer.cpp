#define NOMINMAX

#include "sed1356.h"

#include "sed1356_config.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include "../../host/panel_frame_renderer.h"
#include "../epson_ink_cursor/epson_ink_cursor_layer.h"
#include <cerfcore/lcd/lcd_pixel_expand.h>

#include <cstring>

namespace cerfcore {

namespace {

/* S1D13506 LCD pipe -> host BGRA. Pixel formats per Technical Manual §11.1:
   16 bpp 5-6-5, 15 bpp 5-5-5, 4/8 bpp through the 4-bit LCD LUT. */
class Sed1356Renderer : public PanelFrameRenderer {
public:
    using PanelFrameRenderer::PanelFrameRenderer;

    bool ShouldRegister() override {
        return emu_.TryGet<Sed1356Config>() != nullptr;
    }

    void PresentedSize(uint32_t& w, uint32_t& h) override {
        auto& sed = emu_.Get<Sed1356>();
        w = sed.LcdGuestW();
        h = sed.LcdGuestH();
    }

    bool HasFrame() override {
        auto& sed = emu_.Get<Sed1356>();
        if (!sed.LcdDisplayOn()) return false;
        const uint32_t bpp    = sed.LcdBpp();
        const uint32_t stride = sed.LcdStrideBytes();
        const uint32_t start  = sed.Buffer().Wrap(sed.LcdStartByte());
        if (bpp == 0 || stride == 0) return false;
        const size_t bytes = (size_t)stride * sed.LcdGuestH();
        if (start + bytes > sed.Buffer().Size()) return false;
        return true;
    }

    void RenderInto(uint32_t* dib, uint32_t host_w, uint32_t host_h) override {
        std::memset(dib, 0, (size_t)host_w * host_h * 4u);

        auto& sed = emu_.Get<Sed1356>();
        if (sed.LcdBlanked()) return;          /* REG[040h].7: outputs zeroed. */

        const uint32_t swivel = sed.SwivelMode();
        if (swivel == 1u || swivel == 3u) {    /* Table 8-19: 90/270 degrees. */
            emu_.Get<Fatal>().Die("Sed1356Renderer: SwivelView %u-degree mode is not "
                                  "implemented", swivel * 90u);
        }

        const uint32_t gw     = sed.LcdGuestW(), gh = sed.LcdGuestH();
        const uint32_t bpp    = sed.LcdBpp();
        const uint32_t stride = sed.LcdStrideBytes();
        const uint32_t start  = sed.Buffer().Wrap(sed.LcdStartByte());
        if (gw == 0 || gh == 0 || bpp == 0 || stride == 0) return;

        /* REG[048h] pans the display left; bits used per depth (Table 8-21):
           4 bpp -> [1:0], 8 bpp -> [0], 15/16 bpp -> none. */
        uint32_t pan = sed.LcdPixelPan();
        if (bpp == 8u)       pan &= 0x1u;
        else if (bpp >= 15u) pan = 0;

        const uint8_t* vram = sed.Buffer().Data();

        const uint32_t cw = (host_w < gw) ? host_w : gw;
        const uint32_t ch = (host_h < gh) ? host_h : gh;
        for (uint32_t y = 0; y < ch; ++y) {
            uint32_t* dst = dib + (size_t)y * host_w;
            const uint32_t sy = (swivel == 2u) ? gh - 1u - y : y;
            const uint32_t line = sed.Buffer().Wrap(start + sy * stride);
            for (uint32_t x = 0; x < cw; ++x) {
                const uint32_t sx = ((swivel == 2u) ? gw - 1u - x : x) + pan;
                dst[x] = DecodePixel(sed, vram, line, sx, bpp);
            }
        }

        ComposeInkCursor(sed, dib, host_w, cw, ch, gw, gh);
    }

private:
    static uint32_t FromLut(Sed1356& sed, uint32_t index) {
        uint8_t r4, g4, b4;
        sed.LcdLutRgb(index, r4, g4, b4);
        return lcd_pixel::PackXrgb4(r4, g4, b4);
    }

    static uint32_t DecodePixel(Sed1356& sed, const uint8_t* vram,
                                uint32_t line, uint32_t x, uint32_t bpp) {
        switch (bpp) {
            case 16u:
                return lcd_pixel::Expand565(static_cast<uint16_t>(sed.Buffer().Load(line + x * 2u, 2u)));
            case 15u:                          /* §11.1: 5-5-5. */
                return lcd_pixel::Expand555(static_cast<uint16_t>(sed.Buffer().Load(line + x * 2u, 2u)));
            case 8u:
                return FromLut(sed, vram[sed.Buffer().Wrap(line + x)]);
            default: {                         /* 4 bpp: pixel 0 in bits 7:4. */
                const uint8_t b = vram[sed.Buffer().Wrap(line + x / 2u)];
                return FromLut(sed, (x & 1u) ? (b & 0xFu) : (b >> 4));
            }
        }
    }

    static int32_t Signed(uint32_t magnitude, bool negative) {
        return negative ? -static_cast<int32_t>(magnitude) : static_cast<int32_t>(magnitude);
    }

    static uint32_t InkColor(Sed1356& sed, uint32_t which) {
        uint8_t r5, g6, b5;
        sed.LcdInkColor(which, r5, g6, b5);
        return lcd_pixel::Expand565(static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5));
    }

    static void ComposeInkCursor(Sed1356& sed, uint32_t* dib, uint32_t host_w, uint32_t cw, uint32_t ch,
                                 uint32_t gw, uint32_t gh) {
        const uint32_t mode = sed.LcdInkCursorMode();
        if (mode == 0u || mode == 3u) return;
        const bool          cursor = mode == 1u;
        EpsonInkCursorLayer layer;
        if (cursor) {
            layer.x = Signed(sed.LcdCursorX(), sed.LcdCursorXNeg());
            layer.y = Signed(sed.LcdCursorY(), sed.LcdCursorYNeg());
        }
        layer.width      = cursor ? 64u : gw;
        layer.height     = cursor ? 64u : gh;
        layer.start_byte = sed.LcdInkCursorStartByte();
        layer.line_bytes = cursor ? 16u : (static_cast<uint32_t>(sed.BltReg(0x72)) + 1u) * 2u;
        layer.color0     = InkColor(sed, 0u);
        layer.color1     = InkColor(sed, 1u);
        layer.ComposeInto(sed.Buffer(), dib, host_w, cw, ch);
    }

};

}  /* namespace */

REGISTER_SERVICE_AS(Sed1356Renderer, PanelFrameRenderer);

}
