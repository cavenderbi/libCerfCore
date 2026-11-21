#include "s1d13705.h"

#include "s1d13705_config.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>
#include "../../host/panel_frame_renderer.h"

#include <cstring>

namespace cerfcore {

namespace {

struct ScanLayout {
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t bpp    = 0;
    bool     swivel = false;
    uint32_t screen1_bytes = 0;
    uint32_t screen2_bytes = 0;
    uint32_t split_line    = 0;
    uint32_t pitch_bytes   = 0;
    uint32_t row_a_bytes   = 0;
};

class S1d13705Renderer : public PanelFrameRenderer {
public:
    using PanelFrameRenderer::PanelFrameRenderer;

    bool ShouldRegister() override {
        return emu_.TryGet<S1d13705Config>() != nullptr;
    }

    void PresentedSize(uint32_t& w, uint32_t& h) override {
        auto& lcd = emu_.Get<S1d13705>();
        w = lcd.PanelWidth();
        h = lcd.PanelHeight();
    }

    bool HasFrame() override {
        ScanLayout layout;
        return Layout(emu_.Get<S1d13705>(), layout);
    }

    void RenderInto(uint32_t* dib, uint32_t host_w, uint32_t host_h) override {
        std::memset(dib, 0, static_cast<size_t>(host_w) * host_h * 4u);
        auto&      lcd = emu_.Get<S1d13705>();
        ScanLayout layout;
        if (!Layout(lcd, layout) || lcd.Blanked()) return;
        const uint8_t* vram = lcd.Vram();
        const uint32_t cw   = host_w < layout.width ? host_w : layout.width;
        const uint32_t ch   = host_h < layout.height ? host_h : layout.height;
        for (uint32_t y = 0; y < ch; ++y) {
            uint32_t* dst = dib + static_cast<size_t>(y) * host_w;
            for (uint32_t x = 0; x < cw; ++x) dst[x] = Pixel(lcd, IndexAt(vram, layout, x, y));
        }
    }

private:
    static bool Layout(S1d13705& lcd, ScanLayout& out) {
        if (!lcd.PanelPowered()) return false;
        out.width  = lcd.PanelWidth();
        out.height = lcd.PanelHeight();
        out.bpp    = lcd.Bpp();
        out.swivel = lcd.Swivel();
        return out.swivel ? SwivelLayout(lcd, out) : LandscapeLayout(lcd, out);
    }

    static bool LandscapeLayout(S1d13705& lcd, ScanLayout& out) {
        const uint32_t line_bits = out.width * out.bpp;
        if (line_bits % 16u != 0u) return false;
        out.pitch_bytes   = line_bits / 8u + lcd.AddressOffsetWords() * 2u;
        out.screen1_bytes = lcd.Screen1StartWord() * 2u;
        out.screen2_bytes = lcd.Screen2StartWord() * 2u;
        out.split_line    = lcd.Screen1VerticalSize();
        const uint32_t screen1_lines = out.split_line + 1u < out.height ? out.split_line + 1u : out.height;
        const uint32_t screen2_lines = out.height - screen1_lines;
        if (!Fits(out.screen1_bytes, screen1_lines, out.pitch_bytes, line_bits / 8u)) return false;
        return Fits(out.screen2_bytes, screen2_lines, out.pitch_bytes, line_bits / 8u);
    }

    static bool SwivelLayout(S1d13705& lcd, ScanLayout& out) {
        const uint32_t image_bits = out.height * out.bpp;
        if (image_bits % 8u != 0u || lcd.Screen1VerticalSize() < lcd.PanelHeightField()) return false;
        const uint32_t image_bytes = image_bits / 8u;
        const uint32_t start       = lcd.SwivelStartByte();
        if (start + 1u < image_bytes) return false;
        out.row_a_bytes = start + 1u - image_bytes;
        out.pitch_bytes = lcd.SwivelStrideBytes();
        return Fits(out.row_a_bytes, out.width, out.pitch_bytes, image_bytes);
    }

    static bool Fits(uint32_t base, uint32_t rows, uint32_t pitch, uint32_t row_bytes) {
        if (rows == 0u) return true;
        const uint64_t end = uint64_t(base) + uint64_t(rows - 1u) * pitch + row_bytes;
        return end <= S1d13705::kDisplayBufferBytes;
    }

    static uint32_t IndexAt(const uint8_t* vram, const ScanLayout& l, uint32_t x, uint32_t y) {
        if (l.swivel) {
            const uint8_t* row = vram + l.row_a_bytes + static_cast<size_t>(x) * l.pitch_bytes;
            return lcd_pixel::PackedIndexMsbFirst(row, l.height - 1u - y, l.bpp);
        }
        const uint8_t* row = y <= l.split_line
            ? vram + l.screen1_bytes + static_cast<size_t>(y) * l.pitch_bytes
            : vram + l.screen2_bytes + static_cast<size_t>(y - l.split_line - 1u) * l.pitch_bytes;
        return lcd_pixel::PackedIndexMsbFirst(row, x, l.bpp);
    }

    static uint32_t Pixel(S1d13705& lcd, uint32_t index) {
        uint8_t r4, g4, b4;
        lcd.LutRgb(index, r4, g4, b4);
        if (!lcd.Color()) r4 = b4 = g4;
        if (lcd.Inverted()) {
            r4 ^= 0xFu;
            g4 ^= 0xFu;
            b4 ^= 0xFu;
        }
        return lcd_pixel::PackXrgb(lcd_pixel::Expand4(r4), lcd_pixel::Expand4(g4), lcd_pixel::Expand4(b4));
    }
};

}

REGISTER_SERVICE_AS(S1d13705Renderer, PanelFrameRenderer);

}
