#include "s1d13505.h"

#include "s1d13505_config.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>
#include "../../host/panel_frame_renderer.h"

#include <algorithm>
#include <cstring>

namespace cerfcore {

namespace {

class S1d13505Renderer : public PanelFrameRenderer {
public:
    using PanelFrameRenderer::PanelFrameRenderer;

    bool ShouldRegister() override {
        return emu_.TryGet<S1d13505Config>() != nullptr;
    }

    void PresentedSize(uint32_t& w, uint32_t& h) override {
        auto& lcd = emu_.Get<S1d13505>();
        w = lcd.PanelWidth();
        h = lcd.PanelHeight();
    }

    bool HasFrame() override {
        return emu_.Get<S1d13505>().PanelPowered();
    }

    void RenderInto(uint32_t* dib, uint32_t host_w, uint32_t host_h) override {
        std::memset(dib, 0, static_cast<size_t>(host_w) * host_h * 4u);
        auto& lcd = emu_.Get<S1d13505>();
        if (!lcd.PanelPowered() || lcd.FifoDisabled() || lcd.Bpp() == 0u) return;
        const uint32_t cw    = std::min(host_w, lcd.PanelWidth());
        const uint32_t ch    = std::min(host_h, lcd.PanelHeight());
        const uint32_t split = lcd.Screen1Lines();
        for (uint32_t y = 0; y < ch; ++y) {
            const bool     screen1 = y < split;
            const uint32_t line    = screen1 ? lcd.Screen1StartByte() + y * lcd.StrideBytes()
                                             : lcd.Screen2StartByte() + (y - split) * lcd.StrideBytes();
            const uint32_t pan     = screen1 ? lcd.Screen1Pan() : lcd.Screen2Pan();
            uint32_t*      dst     = dib + static_cast<size_t>(y) * host_w;
            for (uint32_t x = 0; x < cw; ++x) dst[x] = Pixel(lcd, line, pan + x);
        }
        EpsonInkCursorLayer layer;
        if (lcd.InkCursor(layer)) layer.ComposeInto(lcd.Buffer(), dib, host_w, cw, ch);
    }

private:
    static uint32_t Pixel(const S1d13505& lcd, uint32_t line, uint32_t x) {
        const uint32_t            bpp = lcd.Bpp();
        const EpsonDisplayBuffer& buf = lcd.Buffer();
        if (bpp >= 15u) return lcd.DirectXrgb(static_cast<uint16_t>(buf.Load(line + x * 2u, 2u)), bpp);
        const uint32_t bit = x * bpp;
        return lcd.LutXrgb(lcd_pixel::IndexMsbFirst(buf.Load(line + (bit >> 3), 1u), bit, bpp));
    }
};

}

REGISTER_SERVICE_AS(S1d13505Renderer, PanelFrameRenderer);

}
