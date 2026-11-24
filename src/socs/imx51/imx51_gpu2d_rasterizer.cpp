#define NOMINMAX
#include "imx51_gpu2d_rasterizer.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "imx51_id.h"
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/state/state_stream.h>
#include "imx51_gpu2d_gradw_sampler.h"
#include "imx51_gpu2d_pixel_blend.h"
#include <cerfcore/lcd/lcd_pixel_expand.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cerfcore {

REGISTER_SERVICE(Imx51Gpu2dRasterizer);

bool Imx51Gpu2dRasterizer::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetSocId() == SocId::Imx51;
}

void Imx51Gpu2dRasterizer::OnReady() {
    blend_ = &emu_.Get<Imx51Gpu2dPixelBlend>();
}

void Imx51Gpu2dRasterizer::HaltFill(const char* why, float a, float b) const {
    emu_.Get<Fatal>().Die("[GPU2D-RAST] %s (%.4f, %.4f) - not modeled", why, a, b);
}

void Imx51Gpu2dRasterizer::MoveTo(float x, float y, bool closed) {
    starts_.push_back(static_cast<uint32_t>(pts_.size() / 2u));
    closed_.push_back(closed ? 1u : 0u);
    smooth_.push_back(0u);
    pts_.push_back(x);
    pts_.push_back(y);
}

void Imx51Gpu2dRasterizer::LineTo(float x, float y, bool smooth) {
    if (starts_.empty()) HaltFill("LINETO before any MOVETO", x, y);
    smooth_.push_back(smooth ? 1u : 0u);
    pts_.push_back(x);
    pts_.push_back(y);
}

/* gpuaddr==physical (GPU MMU disabled); row byte stride = stride_dw 4-byte words,
   pixel size 1B (G2D_A8), 2B (G2D_0565) or 4B (G2D_8888). An unbacked dest pixel halts. */
uint8_t* Imx51Gpu2dRasterizer::DestHost(const Gpu2dFillTarget& t, int32_t x, int32_t y) {
    const int64_t row = t.sample_index >= 0 ? int64_t(y) * 4 + t.sample_index : y;
    const uint32_t pa = t.dest_pa + static_cast<uint32_t>(row * t.stride_dw * 4)
                      + static_cast<uint32_t>(x * (t.dest_a8 ? 1 : (t.dest_565 || t.dest_5551 || t.dest_4444 || t.dest_argb4444 || t.dest_argb1555) ? 2 : 4));
    uint8_t* hp = emu_.Get<EmulatedMemory>().TryTranslateWrite(pa);
    if (!hp) {
        emu_.Get<Fatal>().Die("[GPU2D-RAST] dest pixel unbacked pa=0x%08X (x=%d y=%d)", pa, x, y);
    }
    return hp;
}

uint32_t Imx51Gpu2dRasterizer::LoadDest(const Gpu2dFillTarget& t, int32_t x, int32_t y) {
    uint8_t* hp = DestHost(t, x, y);
    if (t.dest_a8) return uint32_t(*hp) << 24;
    if (t.dest_565 || t.dest_5551 || t.dest_4444 || t.dest_argb4444 || t.dest_argb1555) {
        const uint16_t pixel = *reinterpret_cast<const uint16_t*>(hp);
        if (t.dest_565) return Gpu2dDecodeTexture16(pixel, 6u);
        if (t.dest_5551) return Gpu2dDecodeTexture16(pixel, 12u);
        if (t.dest_4444) return Gpu2dDecodeTexture16(pixel, 11u);
        return Gpu2dDecodeTexture16(pixel, t.dest_argb4444 ? 4u : 5u);
    }
    return *reinterpret_cast<const uint32_t*>(hp);
}

void Imx51Gpu2dRasterizer::StoreDest(const Gpu2dFillTarget& t, int32_t x, int32_t y,
                                     uint32_t argb) {
    uint8_t* hp = DestHost(t, x, y);
    if (t.dest_a8) *hp = static_cast<uint8_t>(argb >> 24);
    else if (t.dest_argb4444) {
        *reinterpret_cast<uint16_t*>(hp) = static_cast<uint16_t>(((argb >> 16) & 0xF000u) |
            ((argb >> 12) & 0x0F00u) | ((argb >> 8) & 0x00F0u) | ((argb >> 4) & 0x000Fu));
    } else if (t.dest_argb1555) {
        *reinterpret_cast<uint16_t*>(hp) = static_cast<uint16_t>(((argb >> 16) & 0x8000u) |
            ((argb >> 9) & 0x7C00u) | ((argb >> 6) & 0x03E0u) | ((argb >> 3) & 0x001Fu));
    } else if (t.dest_4444) {
        *reinterpret_cast<uint16_t*>(hp) = static_cast<uint16_t>(((argb >> 20) & 15u) << 12 |
            ((argb >> 12) & 15u) << 8 | ((argb >> 4) & 15u) << 4 | (argb >> 28));
    } else if (t.dest_5551) {
        *reinterpret_cast<uint16_t*>(hp) = static_cast<uint16_t>(((argb >> 19) & 31u) << 11 |
            ((argb >> 11) & 31u) << 6 | ((argb >> 3) & 31u) << 1 | (argb >> 31));
    } else if (t.dest_565) *reinterpret_cast<uint16_t*>(hp) = lcd_pixel::PackRgb565(argb);
    else            *reinterpret_cast<uint32_t*>(hp) = argb;
}

uint32_t Imx51Gpu2dRasterizer::PaintSrc(const Gpu2dFillTarget& t,
                                        const Imx51Gpu2dGradwSampler* samp,
                                        int32_t x, int32_t y) const {
    const int32_t paint_y = t.sample_index >= 0 ? y * 4 : y;
    return (t.paint && samp) ? samp->Sample(*t.paint, x, paint_y) : t.argb;
}

uint32_t Imx51Gpu2dRasterizer::BlendAt(const Gpu2dFillTarget& t,
                                       const Imx51Gpu2dGradwSampler* samp,
                                       int32_t x, int32_t y, float coverage) {
    if (t.mask_pa) {
        const int64_t row = t.sample_index >= 0 ? int64_t(y) * 4 + t.sample_index : y;
        const uint32_t pa = t.mask_pa + static_cast<uint32_t>(row * t.mask_stride_dw * 4 + x);
        const uint8_t* mask = emu_.Get<EmulatedMemory>().TryTranslate(pa);
        if (!mask) emu_.Get<Fatal>().Die("[GPU2D-RAST] mask unbacked pa=0x%08X", pa);
        coverage *= static_cast<float>(*mask) / 255.0f;
    }
    const int32_t paint_y = t.sample_index >= 0 ? y * 4 : y;
    // A full-coverage SOURCE replacement cancels premultiplication exactly.
    if (coverage == 1.0f && !t.mask_pa && t.paint && samp && !t.second_paint &&
        !t.paint->premultiply && !t.paint->bilinear && t.blend && t.oo_alpha &&
        !t.image_paint && !t.generic_blend && !t.color_transform &&
        t.prog_a == 0x00812004u && t.prog_c == 0x00812004u) {
        const uint32_t texel = samp->SampleTexel(*t.paint, x, paint_y);
        return (texel >> 24) ? texel : 0u;
    }
    const uint32_t sampled = PaintSrc(t, samp, x, y);
    const uint32_t source = t.second_paint ? samp->Sample(*t.second_paint, x, paint_y) : sampled;
    const uint32_t image = t.second_paint ? sampled : 0xFFFFFFFFu;
    const uint32_t result = blend_->BlendPixel(t, source, LoadDest(t, x, y), coverage, image);
    if (t.blend && !t.oo_alpha && (t.dest_argb1555 || t.dest_5551) && (result >> 24) < 128u)
        return 0u;
    return result;
}

/* Fill one inclusive scanline span [x0,x1] at row y, clipped to the target
   rect: const_px when the blend result is pixel-independent, else per-pixel
   dest read-modify-write. */
void Imx51Gpu2dRasterizer::FillRow(const Gpu2dFillTarget& t, int32_t y, int32_t x0, int32_t x1,
                                   uint32_t const_px, bool per_pixel,
                                   const Imx51Gpu2dGradwSampler* samp) {
    if (y < t.clip_t || y > t.clip_b) return;
    x0 = std::max(x0, t.clip_l);
    x1 = std::min(x1, t.clip_r);
    if (x0 > x1) return;
    for (int32_t x = x0; x <= x1; ++x) {
        if (per_pixel)
            StoreDest(t, x, y,
                      BlendAt(t, samp, x, y, 1.0f));
        else
            StoreDest(t, x, y, const_px);
    }
}

/* Fill rules per openvg_spec.pdf printed p.98 (nonzero: crossing sum != 0;
   even-odd: sum odd); subpaths implicitly closed per printed p.100. */
void Imx51Gpu2dRasterizer::Flush(const Gpu2dFillTarget& t) {
    if (pts_.empty()) { starts_.clear(); closed_.clear(); smooth_.clear(); return; }

    const uint32_t nsub = static_cast<uint32_t>(starts_.size());
    const uint32_t npts = static_cast<uint32_t>(pts_.size() / 2u);

    /* Integral axis-aligned geometry takes the binary scanline (byte-identical
       to the g12 coverage); fractional axis-aligned takes the box-filter path; a
       diagonal edge (a flattened curve, EmitQuad) takes the general 16-subsample
       contour coverage. */
    bool has_diagonal = false;
    for (uint32_t s = 0; s < nsub && !has_diagonal; ++s) {
        const uint32_t first = starts_[s];
        const uint32_t last  = (s + 1u < nsub) ? starts_[s + 1u] : npts;
        for (uint32_t e = first; e < last; ++e) {
            const uint32_t n = e + 1u == last ? first : e + 1u;
            if (pts_[e * 2u] != pts_[n * 2u] && pts_[e * 2u + 1u] != pts_[n * 2u + 1u]) {
                has_diagonal = true;
                break;
            }
        }
    }

    /* Blend programs whose operands never read DEST produce one constant pixel
       (full coverage): fold them once. A GRADW paint varies SOURCE per pixel. */
    const Imx51Gpu2dGradwSampler* samp =
        t.paint ? &emu_.Get<Imx51Gpu2dGradwSampler>() : nullptr;
    uint32_t const_px  = t.argb;
    bool     per_pixel = t.paint != nullptr || t.mask_pa != 0u;
    if (t.blend) {
        if (!t.color_transform && !t.generic_blend) {
            blend_->ValidateBlendProg(t.prog_a);
            blend_->ValidateBlendProg(t.prog_c);
        }
        per_pixel = per_pixel || t.color_transform || t.generic_blend || blend_->BlendProgRefsDest(t.prog_a) ||
                    blend_->BlendProgRefsDest(t.prog_c);
        if (!per_pixel) const_px = blend_->BlendPixel(t, t.argb, 0u, 1.0f);
    }

    if (t.multisample || (has_diagonal && t.antialias)) {  /* general-edge path: one contour per subpath, implicitly closed */
        std::vector<std::vector<float>> contours;
        contours.reserve(nsub);
        for (uint32_t s = 0; s < nsub; ++s) {
            const uint32_t first = starts_[s];
            const uint32_t last  = (s + 1u < nsub) ? starts_[s + 1u] : npts;
            contours.emplace_back(pts_.begin() + first * 2u, pts_.begin() + last * 2u);
        }
        FillContours(t, contours, const_px, per_pixel);
        pts_.clear();
        starts_.clear();
        closed_.clear(); smooth_.clear();
        return;
    }

    bool integral = true;
    for (uint32_t i = 0; i < npts && integral; ++i)
        integral = std::fabs(pts_[i * 2u] - std::round(pts_[i * 2u])) <= 1.0f / 64.0f &&
                   std::fabs(pts_[i * 2u + 1u] - std::round(pts_[i * 2u + 1u])) <= 1.0f / 64.0f;
    if (integral)
        for (uint32_t i = 0; i < npts * 2u; ++i) pts_[i] = std::round(pts_[i]);

    /* Path device-space vertical extent, clipped to the target rect. */
    float miny = pts_[1], maxy = pts_[1];
    for (uint32_t i = 0; i < npts; ++i) {
        miny = std::min(miny, pts_[i * 2u + 1u]);
        maxy = std::max(maxy, pts_[i * 2u + 1u]);
    }
    int32_t y_lo = std::max(t.clip_t, static_cast<int32_t>(std::floor(miny)));
    int32_t y_hi = std::min(t.clip_b, static_cast<int32_t>(std::ceil(maxy)) - 1);

    if (!integral && t.antialias) {
        FlushCoverage(t, const_px, per_pixel, samp);
        pts_.clear();
        starts_.clear();
        closed_.clear(); smooth_.clear();
        return;
    }

    struct Crossing { float x; int dir; };
    std::vector<Crossing> xs;

    for (int32_t y = y_lo; y <= y_hi; ++y) {
        const float sy = static_cast<float>(y) + 0.5f;
        xs.clear();
        for (uint32_t s = 0; s < nsub; ++s) {
            const uint32_t first = starts_[s];
            const uint32_t last  = (s + 1u < nsub) ? starts_[s + 1u] : npts;
            const uint32_t n = last - first;
            if (n < 2u) continue;  /* degenerate subpath: no area */
            for (uint32_t e = 0; e < n; ++e) {
                const uint32_t a = first + e;
                const uint32_t b = first + (e + 1u == n ? 0u : e + 1u);  /* implicit close */
                float ax = pts_[a * 2u], ay = pts_[a * 2u + 1u];
                float bx = pts_[b * 2u], by = pts_[b * 2u + 1u];
                if (ay == by) continue;                       /* horizontal: no crossing */
                int dir = 1;
                if (ay > by) { std::swap(ax, bx); std::swap(ay, by); dir = -1; }
                if (sy < ay || sy >= by) continue;            /* half-open [ay,by) */
                const float x = ax + (bx - ax) * (sy - ay) / (by - ay);
                xs.push_back({x, dir});
            }
        }
        if (xs.empty()) continue;
        std::sort(xs.begin(), xs.end(),
                  [](const Crossing& p, const Crossing& q) { return p.x < q.x; });

        int wind = 0;
        for (size_t i = 0; i + 1u < xs.size(); ++i) {
            wind += xs[i].dir;
            const bool inside = t.even_odd ? ((i + 1u) & 1u) != 0u : (wind != 0);
            if (!inside) continue;
            /* Span [xs[i].x, xs[i+1].x): pixel centers x+0.5 inside the span. */
            const int32_t px0 = static_cast<int32_t>(std::ceil(xs[i].x - 0.5f));
            const int32_t px1 = static_cast<int32_t>(std::ceil(xs[i + 1u].x - 0.5f)) - 1;
            FillRow(t, y, px0, px1, const_px, per_pixel, samp);
        }
    }

    pts_.clear();
    starts_.clear();
    closed_.clear(); smooth_.clear();
}

/* Exact box-filter coverage for a fractional rectilinear path (openvg_spec.pdf
   "Coverage Computation" stage; its Flash-player note mandates single-pixel box
   resolution): the vertical-edge crossing set is constant between vertex-y
   breaks, so pixel coverage = sum of segment-height * x-span overlaps. */
void Imx51Gpu2dRasterizer::FlushCoverage(const Gpu2dFillTarget& t,
                                         uint32_t const_px, bool per_pixel,
                                         const Imx51Gpu2dGradwSampler* samp) {
    const uint32_t nsub = static_cast<uint32_t>(starts_.size());
    const uint32_t npts = static_cast<uint32_t>(pts_.size() / 2u);

    std::vector<float> ybreaks;
    ybreaks.reserve(npts);
    for (uint32_t i = 0; i < npts; ++i) ybreaks.push_back(pts_[i * 2u + 1u]);
    std::sort(ybreaks.begin(), ybreaks.end());
    ybreaks.erase(std::unique(ybreaks.begin(), ybreaks.end()), ybreaks.end());

    const int32_t y_lo = std::max(t.clip_t, static_cast<int32_t>(std::floor(ybreaks.front())));
    const int32_t y_hi = std::min(t.clip_b,
                                  static_cast<int32_t>(std::ceil(ybreaks.back())) - 1);
    const int32_t ncols = t.clip_r - t.clip_l + 1;
    if (ncols <= 0) return;
    std::vector<float> cov(static_cast<size_t>(ncols));

    struct Crossing { float x; int dir; };
    std::vector<Crossing> xs;

    for (int32_t y = y_lo; y <= y_hi; ++y) {
        std::fill(cov.begin(), cov.end(), 0.0f);
        const float ry0 = static_cast<float>(y), ry1 = ry0 + 1.0f;
        for (size_t k = 0; k + 1u < ybreaks.size(); ++k) {
            const float ya = std::max(ybreaks[k], ry0), yb = std::min(ybreaks[k + 1u], ry1);
            if (yb <= ya) continue;
            const float h = yb - ya, my = 0.5f * (ya + yb);
            xs.clear();
            for (uint32_t s = 0; s < nsub; ++s) {
                const uint32_t first = starts_[s];
                const uint32_t n = ((s + 1u < nsub) ? starts_[s + 1u] : npts) - first;
                if (n < 2u) continue;  /* degenerate subpath: no area */
                for (uint32_t e = 0; e < n; ++e) {
                    const uint32_t a = first + e;
                    const uint32_t b = first + (e + 1u == n ? 0u : e + 1u);  /* implicit close */
                    float ay = pts_[a * 2u + 1u], by = pts_[b * 2u + 1u];
                    if (ay == by) continue;               /* horizontal: no crossing */
                    int dir = 1;
                    if (ay > by) { std::swap(ay, by); dir = -1; }
                    if (my < ay || my >= by) continue;    /* half-open [ay,by) */
                    xs.push_back({pts_[a * 2u], dir});    /* axis-aligned: ax == bx */
                }
            }
            if (xs.empty()) continue;
            std::sort(xs.begin(), xs.end(),
                      [](const Crossing& p, const Crossing& q) { return p.x < q.x; });
            int wind = 0;
            for (size_t i = 0; i + 1u < xs.size(); ++i) {
                wind += xs[i].dir;
                const bool inside = t.even_odd ? ((i + 1u) & 1u) != 0u : (wind != 0);
                if (!inside) continue;
                const float x0 = std::max(xs[i].x, static_cast<float>(t.clip_l));
                const float x1 = std::min(xs[i + 1u].x, static_cast<float>(t.clip_r + 1));
                for (int32_t ix = static_cast<int32_t>(std::floor(x0));
                     ix < static_cast<int32_t>(std::ceil(x1)); ++ix) {
                    const float w = std::min(x1, static_cast<float>(ix + 1)) -
                                    std::max(x0, static_cast<float>(ix));
                    if (w > 0.0f) cov[static_cast<size_t>(ix - t.clip_l)] += h * w;
                }
            }
        }
        for (int32_t ix = 0; ix < ncols; ++ix) {
            const float c = std::min(cov[static_cast<size_t>(ix)], 1.0f);
            if (c <= 1.0f / 512.0f) continue;
            const int32_t x = t.clip_l + ix;
            if (c >= 1.0f - 1.0f / 512.0f) {
                if (per_pixel)
                    StoreDest(t, x, y, BlendAt(t, samp, x, y, 1.0f));
                else
                    StoreDest(t, x, y, const_px);
                continue;
            }
            if (!t.blend)  /* partial coverage composes only through the blender */
                HaltFill("partial coverage with blender disabled", c, static_cast<float>(x));
            StoreDest(t, x, y,
                      BlendAt(t, samp, x, y, c));
        }
    }
}

/* Coverage of arbitrary-edge contours under t.even_odd/nonzero winding: 16
   sub-scanlines per pixel row (VGSPAN.COVERAGE[4] = the g12's 1/16 vertical
   resolution) with exact horizontal span overlap. Partial pixels compose only
   through the blender. */
void Imx51Gpu2dRasterizer::FillContours(const Gpu2dFillTarget& t,
                                        const std::vector<std::vector<float>>& contours,
                                        uint32_t const_px, bool per_pixel) {
    if (contours.empty()) return;
    if (t.multisample || !t.antialias) {
        FillSampledContours(t, contours, const_px, per_pixel);
        return;
    }
    float miny = 1e30f, maxy = -1e30f;
    for (const auto& c : contours)
        for (size_t i = 0; i < c.size() / 2u; ++i) {
            miny = std::min(miny, c[i * 2u + 1u]);
            maxy = std::max(maxy, c[i * 2u + 1u]);
        }
    const int32_t y_lo = std::max(t.clip_t, static_cast<int32_t>(std::floor(miny)));
    const int32_t y_hi = std::min(t.clip_b, static_cast<int32_t>(std::ceil(maxy)) - 1);
    const int32_t ncols = t.clip_r - t.clip_l + 1;
    if (ncols <= 0) return;
    std::vector<float> cov(static_cast<size_t>(ncols));
    const Imx51Gpu2dGradwSampler* samp =
        t.paint ? &emu_.Get<Imx51Gpu2dGradwSampler>() : nullptr;

    constexpr int kSub = 16;
    struct Crossing { float x; int dir; };
    std::vector<Crossing> xs;

    for (int32_t y = y_lo; y <= y_hi; ++y) {
        std::fill(cov.begin(), cov.end(), 0.0f);
        for (int k = 0; k < kSub; ++k) {
            const float sy = static_cast<float>(y) + (static_cast<float>(k) + 0.5f) / kSub;
            xs.clear();
            for (const auto& c : contours) {
                const uint32_t m = static_cast<uint32_t>(c.size() / 2u);
                for (uint32_t e = 0; e < m; ++e) {
                    const uint32_t a = e, b = (e + 1u == m) ? 0u : e + 1u;
                    float ax = c[a * 2u], ay = c[a * 2u + 1u];
                    float bx = c[b * 2u], by = c[b * 2u + 1u];
                    if (ay == by) continue;
                    int dir = 1;
                    if (ay > by) { std::swap(ax, bx); std::swap(ay, by); dir = -1; }
                    if (sy < ay || sy >= by) continue;
                    xs.push_back({ax + (bx - ax) * (sy - ay) / (by - ay), dir});
                }
            }
            if (xs.empty()) continue;
            std::sort(xs.begin(), xs.end(),
                      [](const Crossing& p, const Crossing& q) { return p.x < q.x; });
            int wind = 0;
            for (size_t i = 0; i + 1u < xs.size(); ++i) {
                wind += xs[i].dir;
                const bool inside = t.even_odd ? ((i + 1u) & 1u) != 0u : (wind != 0);
                if (!inside) continue;
                const float x0 = std::max(xs[i].x, static_cast<float>(t.clip_l));
                const float x1 = std::min(xs[i + 1u].x, static_cast<float>(t.clip_r + 1));
                for (int32_t ix = static_cast<int32_t>(std::floor(x0));
                     ix < static_cast<int32_t>(std::ceil(x1)); ++ix) {
                    const float w = std::min(x1, static_cast<float>(ix + 1)) -
                                    std::max(x0, static_cast<float>(ix));
                    if (w > 0.0f) cov[static_cast<size_t>(ix - t.clip_l)] += w / kSub;
                }
            }
        }
        for (int32_t ix = 0; ix < ncols; ++ix) {
            const float c = std::min(cov[static_cast<size_t>(ix)], 1.0f);
            if (c <= 1.0f / 512.0f) continue;
            const int32_t x = t.clip_l + ix;
            if (c >= 1.0f - 1.0f / 512.0f) {
                if (per_pixel)
                    StoreDest(t, x, y, BlendAt(t, samp, x, y, 1.0f));
                else
                    StoreDest(t, x, y, const_px);
                continue;
            }
            if (!t.blend)
                HaltFill("stroke partial coverage with blender disabled", c, static_cast<float>(x));
            StoreDest(t, x, y,
                      BlendAt(t, samp, x, y, c));
        }
    }
}

/* Non-antialiased coverage samples the pixel center. Four-sample coverage
   uses an approximate rotated grid; hardware flipquad alternates and shares
   samples between neighboring pixels. */
void Imx51Gpu2dRasterizer::FillSampledContours(
    const Gpu2dFillTarget& t, const std::vector<std::vector<float>>& contours,
    uint32_t const_px, bool per_pixel) {
    constexpr float offsets[4][2] = {{0.375f, 0.125f}, {0.875f, 0.375f},
                                     {0.125f, 0.625f}, {0.625f, 0.875f}};
    float miny = 1e30f, maxy = -1e30f;
    for (const auto& contour : contours)
        for (size_t i = 1; i < contour.size(); i += 2) {
            miny = std::min(miny, contour[i]);
            maxy = std::max(maxy, contour[i]);
        }
    if (miny > maxy) return;
    const int32_t y_lo = std::max(t.clip_t, static_cast<int32_t>(std::floor(miny)));
    const int32_t y_hi = std::min(t.clip_b, static_cast<int32_t>(std::ceil(maxy)) - 1);
    const Imx51Gpu2dGradwSampler* samp =
        t.paint ? &emu_.Get<Imx51Gpu2dGradwSampler>() : nullptr;
    struct Crossing { float x; int dir; };
    std::vector<Crossing> crossings;
    const int sample_count = t.multisample ? 4 : 1;
    for (int lane = 0; lane < sample_count; ++lane) {
        Gpu2dFillTarget sample = t;
        sample.multisample = false;
        if (t.multisample) {
            sample.sample_index = lane;
            const int32_t sample_top = t.sample_scissor_top <= lane ? 0 :
                (t.sample_scissor_top - lane + 3) / 4;
            const int32_t sample_bottom = t.sample_scissor_bottom < lane ? -1 :
                (t.sample_scissor_bottom - lane) / 4;
            sample.clip_t = std::max(sample.clip_t, sample_top);
            sample.clip_b = std::min(sample.clip_b, sample_bottom);
        }
        const float sample_x = t.multisample ? offsets[lane][0] : 0.5f;
        const float sample_y = t.multisample ? offsets[lane][1] : 0.5f;
        for (int32_t y = std::max(y_lo, sample.clip_t);
             y <= std::min(y_hi, sample.clip_b); ++y) {
            const float sy = static_cast<float>(y) + sample_y;
            crossings.clear();
            for (const auto& contour : contours) {
                const size_t count = contour.size() / 2u;
                for (size_t edge = 0; edge < count; ++edge) {
                    const size_t next = (edge + 1u) % count;
                    float ax = contour[edge * 2u], ay = contour[edge * 2u + 1u];
                    float bx = contour[next * 2u], by = contour[next * 2u + 1u];
                    if (ay == by) continue;
                    int direction = 1;
                    if (ay > by) { std::swap(ax, bx); std::swap(ay, by); direction = -1; }
                    if (sy < ay || sy >= by) continue;
                    // Approximate the VGV1 subpixel edge precision at each sampled crossing.
                    const float crossing = ax + (bx - ax) * (sy - ay) / (by - ay);
                    crossings.push_back({std::round(crossing * 16.0f) / 16.0f, direction});
                }
            }
            std::sort(crossings.begin(), crossings.end(),
                      [](const Crossing& a, const Crossing& b) { return a.x < b.x; });
            int winding = 0;
            for (size_t i = 0; i + 1u < crossings.size(); ++i) {
                winding += crossings[i].dir;
                if (!(t.even_odd ? ((i + 1u) & 1u) != 0u : winding != 0)) continue;
                const float left = std::max(crossings[i].x, static_cast<float>(t.clip_l));
                const float right = std::min(crossings[i + 1u].x, static_cast<float>(t.clip_r + 1));
                if (left >= right) continue;
                const int32_t x0 = static_cast<int32_t>(std::ceil(left - sample_x));
                const int32_t x1 = static_cast<int32_t>(std::ceil(right - sample_x)) - 1;
                FillRow(sample, y, x0, x1, const_px, per_pixel, samp);
            }
        }
    }
}

void Imx51Gpu2dRasterizer::FillRect(const Gpu2dFillTarget& t, int32_t x0, int32_t y0,
                                    int32_t w, int32_t h) {
    if (w <= 0 || h <= 0) return;  /* zero/negative-area rect writes no pixels */
    for (int32_t y = y0; y < y0 + h; ++y)
        FillRow(t, y, x0, x0 + w - 1, t.argb, false, nullptr);
}


void Imx51Gpu2dRasterizer::SaveState(StateWriter& w) const {
    const uint32_t np = static_cast<uint32_t>(pts_.size());
    w.Write("coordinate_count", np);
    for (float f : pts_) w.Write("pts", f);
    const uint32_t ns = static_cast<uint32_t>(starts_.size());
    w.Write("subpath_count", ns);
    for (uint32_t s : starts_) w.Write("starts", s);
    for (uint8_t c : closed_) w.Write("closed", c);
}

void Imx51Gpu2dRasterizer::RestoreState(StateReader& r) {
    uint32_t np = 0;
    r.Read("coordinate_count", np);
    pts_.resize(np);
    for (uint32_t i = 0; i < np; ++i) r.Read("pts", pts_[i]);
    uint32_t ns = 0;
    r.Read("subpath_count", ns);
    starts_.resize(ns);
    for (uint32_t i = 0; i < ns; ++i) r.Read("starts", starts_[i]);
    closed_.resize(ns);
    for (uint32_t i = 0; i < ns; ++i) r.Read("closed", closed_[i]);
}

/* Appended after the existing GPU state so older snapshots remain readable. */
void Imx51Gpu2dRasterizer::SaveSmoothState(StateWriter& w) const {
    const uint32_t count = static_cast<uint32_t>(smooth_.size());
    w.Write("smooth_point_count", count);
    for (uint8_t value : smooth_) w.Write("smooth_point", value);
}

void Imx51Gpu2dRasterizer::RestoreSmoothState(StateReader& r) {
    smooth_.assign(pts_.size() / 2u, 0u);
    if (r.Remaining() == 0) return;
    uint32_t count = 0;
    r.Read("smooth_point_count", count);
    if (count != smooth_.size()) r.Reject("GPU2D smooth point count does not match geometry");
    for (uint8_t& value : smooth_) r.Read("smooth_point", value);
}

}
