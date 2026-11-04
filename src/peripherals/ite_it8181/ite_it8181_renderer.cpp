#define NOMINMAX

#include "ite_it8181.h"

#include "ite_it8181_config.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/panel_frame_renderer.h>
#include <cerfcore/lcd/lcd_pixel_expand.h>

#include <algorithm>

namespace cerfcore {

namespace {

constexpr uint32_t kCursorSide      = 32u;
constexpr uint32_t kCursorRowBytes  = 4u;
constexpr uint32_t kCursorPlane     = kCursorSide * kCursorRowBytes;
constexpr uint32_t kBlack           = 0xFF000000u;
constexpr uint32_t kWhite           = 0xFFFFFFFFu;
constexpr uint32_t kRgbBits         = 0x00FFFFFFu;

class IteIt8181Renderer : public PanelFrameRenderer {
public:
    using PanelFrameRenderer::PanelFrameRenderer;

    bool ShouldRegister() override {
        return emu_.TryGet<IteIt8181Config>() != nullptr;
    }

    void OnReady() override {
        chip_ = &emu_.Get<IteIt8181>();
    }

    void PresentedSize(uint32_t& w, uint32_t& h) override {
        const IteIt8181::Scanout s = chip_->Display();
        w = s.width;
        h = s.height;
    }

    bool HasFrame() override {
        const IteIt8181::Scanout s = chip_->Display();
        if (!s.shown) return false;
        if (latch_.Latched()) return true;
        return latch_.ProbeAndLatch(chip_->Vram() + s.start,
                                    static_cast<size_t>(s.height - 1u) * s.stride +
                                        static_cast<size_t>(s.width) * s.bytes);
    }

    void RenderInto(uint32_t* dib, uint32_t host_w, uint32_t host_h) override {
        std::fill(dib, dib + static_cast<size_t>(host_w) * host_h, 0u);
        const IteIt8181::Scanout s = chip_->Display();
        if (!s.shown) return;
        const uint32_t w = std::min(host_w, s.width);
        const uint32_t h = std::min(host_h, s.height);
        const uint8_t* vram = chip_->Vram();
        for (uint32_t y = 0; y < h; ++y) {
            const uint8_t* line = vram + s.start + static_cast<size_t>(y) * s.stride;
            uint32_t*      dst  = dib + static_cast<size_t>(y) * host_w;
            for (uint32_t x = 0; x < w; ++x) dst[x] = Pixel(line + static_cast<size_t>(x) * s.bytes, s.bytes);
        }
        if (chip_->Gui().CursorShown()) DrawCursor(dib, host_w, w, h);
    }

private:
    uint32_t Pixel(const uint8_t* p, uint32_t bytes) const {
        switch (bytes) {
            case 1u: return chip_->PaletteXrgb(p[0]);
            case 2u: return lcd_pixel::Expand565(static_cast<uint16_t>(p[0] | (p[1] << 8)));
            default: return lcd_pixel::PackXrgb(p[2], p[1], p[0]);
        }
    }

    void DrawCursor(uint32_t* dib, uint32_t host_w, uint32_t w, uint32_t h) const {
        const IteIt8181Gui& gui = chip_->Gui();
        const uint8_t* img = chip_->Vram() + IteIt8181::kCursorImage;
        for (uint32_t cy = 0; cy < kCursorSide; ++cy) {
            const uint32_t sy = gui.CursorY() + cy;
            if (sy >= h) break;
            for (uint32_t cx = 0; cx < kCursorSide; ++cx) {
                const uint32_t sx = gui.CursorX() + cx;
                if (sx >= w) break;
                const uint32_t at      = cy * kCursorRowBytes + cx / 8u;
                const uint32_t shift   = 7u - (cx & 7u);
                const uint32_t and_bit = (img[at] >> shift) & 1u;
                const uint32_t xor_bit = (img[kCursorPlane + at] >> shift) & 1u;
                uint32_t& px = dib[static_cast<size_t>(sy) * host_w + sx];
                if (and_bit) {
                    if (xor_bit) px ^= kRgbBits;
                    continue;
                }
                const bool white = xor_bit ? gui.CursorColor1White() : gui.CursorColor0White();
                px = white ? kWhite : kBlack;
            }
        }
    }

    IteIt8181* chip_ = nullptr;
};

}

REGISTER_SERVICE_AS(IteIt8181Renderer, PanelFrameRenderer);

}
