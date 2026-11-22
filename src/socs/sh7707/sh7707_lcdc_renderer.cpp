#define NOMINMAX

#include "sh7707_lcdc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>
#include "../../host/panel_frame_renderer.h"
#include "sh7707_id.h"
#include "sh7707_lcd_panel.h"

#include <algorithm>
#include <cstring>

namespace cerfcore {

namespace {

uint32_t Gray(uint32_t level) { return lcd_pixel::PackXrgb(level, level, level); }

uint32_t Gradation(uint32_t number) { return lcd_pixel::Expand4(15u - (number & 0xFu)); }

uint32_t PaletteColor(uint32_t mode, uint16_t entry) {
    if (mode == Sh7707Lcdc::kModeStnColor) {
        return lcd_pixel::PackXrgb(Gradation(entry >> 8), Gradation(entry >> 4), Gradation(entry));
    }
    if (mode == Sh7707Lcdc::kModeTft) {
        return lcd_pixel::PackXrgb(lcd_pixel::Expand2((entry >> 4) & 3u), lcd_pixel::Expand2((entry >> 2) & 3u),
                                   lcd_pixel::Expand2(entry & 3u));
    }
    return Gray(Gradation(entry));
}

uint32_t DataLinesLowColor(uint32_t mode) {
    return mode == Sh7707Lcdc::kModeTft ? lcd_pixel::PackXrgb(0u, 0u, 0u) : Gray(Gradation(0u));
}

class Sh7707LcdcRenderer : public PanelFrameRenderer {
public:
    using PanelFrameRenderer::PanelFrameRenderer;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Sh7707; }

    void OnReady() override {
        lcdc_  = &emu_.Get<Sh7707Lcdc>();
        panel_ = &emu_.Get<Sh7707LcdPanel>();
        mem_   = &emu_.Get<EmulatedMemory>();
    }

    void PresentedSize(uint32_t& w, uint32_t& h) override {
        const Sh7707Lcdc::Scanout s = lcdc_->GetScanout();
        w = s.dots;
        h = s.PresentedLines();
    }

    bool HasFrame() override { return lcdc_->GetScanout().panel_lit; }

    void RenderInto(uint32_t* dib_bgra32, uint32_t host_w, uint32_t host_h) override {
        const Sh7707Lcdc::Scanout s = lcdc_->GetScanout();
        std::memset(dib_bgra32, 0, (size_t)host_w * host_h * 4u);
        if (!s.panel_lit) return;

        const uint32_t copy_w = std::min(s.dots, host_w);
        const uint32_t copy_h = std::min(s.PresentedLines(), host_h);
        if (!s.data_out) {
            const uint32_t idle = PanelColor(DataLinesLowColor(s.mode));
            for (uint32_t y = 0; y < copy_h; ++y) {
                uint32_t* dst = dib_bgra32 + (size_t)y * host_w;
                std::fill(dst, dst + copy_w, idle);
            }
            return;
        }

        uint32_t colors[16];
        for (uint32_t i = 0; i < 16u; ++i) colors[i] = PanelColor(PaletteColor(s.mode, s.palette[i]));
        const uint64_t panel_bytes = uint64_t{s.dots} * s.panel_lines * s.bits_per_dot / 8u;
        const uint8_t* upper       = mem_->TryTranslateRange(s.upper_pa, panel_bytes);
        const uint8_t* lower       = s.dual ? mem_->TryTranslateRange(s.lower_pa, panel_bytes) : nullptr;
        const uint64_t swap        = s.linear_bytes ? 0u : 3u;
        for (uint32_t y = 0; y < copy_h; ++y) {
            const bool     lower_half = y >= s.panel_lines;
            const uint8_t* fb         = lower_half ? lower : upper;
            const uint64_t line       = lower_half ? y - s.panel_lines : y;
            uint32_t*      dst        = dib_bgra32 + (size_t)y * host_w;
            for (uint32_t x = 0; x < copy_w; ++x) {
                const uint64_t bit = (line * s.dots + x) * s.bits_per_dot;
                dst[x] = colors[lcd_pixel::IndexMsbFirst(fb[(bit >> 3) ^ swap], static_cast<uint32_t>(bit), s.bits_per_dot)];
            }
        }
    }

private:
    uint32_t PanelColor(uint32_t color) const { return panel_->ShowsColor() ? color : Gray(lcd_pixel::Luma8(color)); }

    Sh7707Lcdc*     lcdc_  = nullptr;
    Sh7707LcdPanel* panel_ = nullptr;
    EmulatedMemory* mem_   = nullptr;
};

}

REGISTER_SERVICE_AS(Sh7707LcdcRenderer, PanelFrameRenderer);

}
