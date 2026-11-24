#define NOMINMAX
#include "imx51_gpu2d_stroker.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "imx51_id.h"
#include "imx51_gpu2d_pixel_blend.h"
#include "imx51_gpu2d_rasterizer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cerfcore {

REGISTER_SERVICE(Imx51Gpu2dStroker);

bool Imx51Gpu2dStroker::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetSocId() == SocId::Imx51;
}

namespace {

constexpr float kPi = 3.14159265358979323846f;

/* A round cap is a semicircle of radius half the line width. The inner
   half of the disc overlaps the segment body. */
void AddDisc(std::vector<std::vector<float>>& out, float cx, float cy, float r, int n) {
    std::vector<float> c;
    c.reserve(static_cast<size_t>(n) * 2u);
    for (int i = 0; i < n; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(n);
        c.push_back(cx + r * std::cos(a));
        c.push_back(cy + r * std::sin(a));
    }
    out.push_back(std::move(c));
}

/* Add only the outer circular sector between the two segment offsets. A full
   disc would extend past a nearby butt cap on short or finely flattened paths. */
void AddRoundJoin(std::vector<std::vector<float>>& out, float ax, float ay,
                  float vx, float vy, float bx, float by, float radius, int segments) {
    float ux = vx - ax, uy = vy - ay, wx = bx - vx, wy = by - vy;
    const float before = std::sqrt(ux * ux + uy * uy), after = std::sqrt(wx * wx + wy * wy);
    if (before < 1e-6f || after < 1e-6f) return;
    ux /= before; uy /= before; wx /= after; wy /= after;
    const float turn = ux * wy - uy * wx;
    const float dot = ux * wx + uy * wy;
    if (std::fabs(turn) < 1e-6f && dot > 0.0f) return;
    const float side = turn >= 0.0f ? 1.0f : -1.0f;
    const float start = std::atan2(-side * ux, side * uy);
    const float sweep = std::atan2(turn, dot);
    const int steps = std::max(1, static_cast<int>(std::ceil(segments * std::fabs(sweep) / (2.0f * kPi))));
    std::vector<float> contour = {vx, vy};
    for (int i = 0; i <= steps; ++i) {
        const float angle = start + sweep * static_cast<float>(i) / static_cast<float>(steps);
        contour.push_back(vx + radius * std::cos(angle));
        contour.push_back(vy + radius * std::sin(angle));
    }
    out.push_back(std::move(contour));
}

// Sweep the stroke's perpendicular cross-section through smooth subdivisions.
// Reversals fall back to cusp handling because their tangent is ambiguous.
bool CanSweepSmoothPath(const std::vector<float>& points, const std::vector<uint8_t>& smooth,
                        bool closed, uint32_t cap) {
    const size_t count = points.size() / 2u;
    if (closed || cap != 0u || count <= 2u ||
        !std::all_of(smooth.begin() + 1, smooth.end() - 1, [](uint8_t value) { return value != 0; }))
        return false;
    for (size_t i = 1; i + 1u < count; ++i) {
        const float before_x = points[i * 2u] - points[(i - 1u) * 2u];
        const float before_y = points[i * 2u + 1u] - points[(i - 1u) * 2u + 1u];
        const float after_x = points[(i + 1u) * 2u] - points[i * 2u];
        const float after_y = points[(i + 1u) * 2u + 1u] - points[i * 2u + 1u];
        if (before_x * after_x + before_y * after_y <
            -0.8f * std::hypot(before_x, before_y) * std::hypot(after_x, after_y)) return false;
    }
    return true;
}

void AddSmoothPenSweep(std::vector<std::vector<float>>& contours,
                       const std::vector<float>& points, float radius) {
    const size_t count = points.size() / 2u;
    std::vector<float> left, right;
    left.reserve(points.size()); right.reserve(points.size());
    for (size_t i = 0; i < count; ++i) {
        const size_t prev = i == 0u ? 0u : i - 1u;
        const size_t next = i + 1u == count ? i : i + 1u;
        float before_x = points[i*2u] - points[prev*2u], before_y = points[i*2u+1u] - points[prev*2u+1u];
        float after_x = points[next*2u] - points[i*2u], after_y = points[next*2u+1u] - points[i*2u+1u];
        float before_len = std::hypot(before_x, before_y), after_len = std::hypot(after_x, after_y);
        if (before_len == 0.0f) { before_x = after_x; before_y = after_y; before_len = after_len; }
        if (after_len == 0.0f) { after_x = before_x; after_y = before_y; after_len = before_len; }
        before_x /= before_len; before_y /= before_len;
        after_x /= after_len; after_y /= after_len;
        float normal_x = before_y + after_y, normal_y = -before_x - after_x;
        const float normal_len = std::hypot(normal_x, normal_y);
        if (normal_len < 1e-5f) { normal_x = before_y; normal_y = -before_x; }
        else { normal_x /= normal_len; normal_y /= normal_len; }
        left.push_back(points[i*2u] + normal_x*radius); left.push_back(points[i*2u+1u] + normal_y*radius);
        right.push_back(points[i*2u] - normal_x*radius); right.push_back(points[i*2u+1u] - normal_y*radius);
    }
    for (size_t i = 0; i + 1u < count; ++i) {
        const size_t j = i + 1u;
        contours.push_back({left[i*2u], left[i*2u+1u], right[i*2u], right[i*2u+1u], left[j*2u], left[j*2u+1u]});
        contours.push_back({right[i*2u], right[i*2u+1u], right[j*2u], right[j*2u+1u], left[j*2u], left[j*2u+1u]});
    }
}

float SignedArea2(const std::vector<float>& c) {
    float a = 0.0f;
    const size_t m = c.size() / 2u;
    for (size_t i = 0; i < m; ++i) {
        const size_t j = (i + 1u == m) ? 0u : i + 1u;
        a += c[i * 2u] * c[j * 2u + 1u] - c[j * 2u] * c[i * 2u + 1u];
    }
    return a;
}

/* g12 reg 0x66 = -cos(2*asin(1/miterLimit)) (sub_41C5B1B8), so spec 8.7.3's
   "miter length 1/sin(theta/2) > limit -> bevel" reduces to: bevel iff
   dot(u,w) < miter_thresh (trapezoid to the apex, else the bevel triangle). */
void AddMiterJoin(std::vector<std::vector<float>>& out, float ax, float ay,
                  float vx, float vy, float bx, float by, float r, float miter_thresh) {
    float ux = vx - ax, uy = vy - ay, wx = bx - vx, wy = by - vy;
    const float lu = std::sqrt(ux * ux + uy * uy), lw = std::sqrt(wx * wx + wy * wy);
    if (lu < 1e-6f || lw < 1e-6f) return;
    ux /= lu; uy /= lu; wx /= lw; wy /= lw;
    const float turn = ux * wy - uy * wx;          /* cross(u,w): >0 left turn, <0 right */
    if (std::fabs(turn) < 1e-6f) return;           /* collinear: no join gap */
    const float sgn = (turn >= 0.0f) ? 1.0f : -1.0f;  /* outer side of the turn */
    const float p1x = vx + sgn * uy * r, p1y = vy - sgn * ux * r;  /* outer end, incoming quad */
    const float p2x = vx + sgn * wy * r, p2y = vy - sgn * wx * r;  /* outer start, outgoing quad */
    if (ux * wx + uy * wy >= miter_thresh) {       /* within limit -> trapezoid to apex */
        const float t = ((p2x - p1x) * wy - (p2y - p1y) * wx) / turn;  /* P1+t*u == P2+s*w */
        out.push_back({vx, vy, p1x, p1y, p1x + t * ux, p1y + t * uy, p2x, p2y});
    } else {                                       /* miter length exceeds limit -> bevel */
        out.push_back({vx, vy, p1x, p1y, p2x, p2y});
    }
}

}  /* namespace */

/* Dilate the accumulated centerline into its stroke outline (spec 8.7.4) and
   coverage-fill it. Build the outline in path space and transform it to device space. */
void Imx51Gpu2dStroker::Stroke(const Gpu2dFillTarget& t, float radius_raw,
                               float xf_a, float xf_b, float xf_c, float xf_d,
                               float exp_scale, uint32_t cap, uint32_t join,
                               float miter_thresh) {
    auto&       rast  = emu_.Get<Imx51Gpu2dRasterizer>();
    const auto& blend = emu_.Get<Imx51Gpu2dPixelBlend>();
    if (cap > 2u || join > 2u)
        rast.HaltFill("stroke cap/join style (not modeled)",
                      static_cast<float>(cap), static_cast<float>(join));
    const float a = xf_a * exp_scale, b = xf_b * exp_scale;
    const float c = xf_c * exp_scale, d = xf_d * exp_scale;
    const float det = a * d - b * c;
    if (det == 0.0f) { rast.ClearPath(); return; }
    const float radius = radius_raw;
    if (radius <= 0.0f) rast.HaltFill("stroke non-positive radius", radius, 0.0f);

    const std::vector<float>&    pts    = rast.PathPoints();
    const std::vector<uint32_t>& starts = rast.PathStarts();
    const std::vector<uint8_t>&  closed = rast.PathClosed();
    const auto& smooth = rast.PathSmooth();
    const uint32_t nsub = static_cast<uint32_t>(starts.size());
    const uint32_t npts = static_cast<uint32_t>(pts.size() / 2u);
    const float pen_scale = std::sqrt(a*a + b*b + c*c + d*d);
    const int arc_segments = std::clamp(static_cast<int>(std::ceil(kPi * std::sqrt(8.0f * radius * pen_scale))), 16, 256);

    std::vector<std::vector<float>> contours;
    for (uint32_t s = 0; s < nsub; ++s) {
        const uint32_t first = starts[s];
        const uint32_t last  = (s + 1u < nsub) ? starts[s + 1u] : npts;

        std::vector<uint8_t> smooth_points;
        std::vector<float> p;  /* subpath with zero-length segments removed (spec 8.7.4) */
        for (uint32_t e = first; e < last; ++e) {
            const float device_x = pts[e * 2u], device_y = pts[e * 2u + 1u];
            const float x = (d * device_x - b * device_y) / det;
            const float y = (a * device_y - c * device_x) / det;
            if (p.size() >= 2u && std::fabs(p[p.size() - 2u] - x) < 1e-7f &&
                std::fabs(p.back() - y) < 1e-7f) {
                smooth_points.back() &= smooth[e];
                continue;
            }
            smooth_points.push_back(smooth[e]);
            p.push_back(x);
            p.push_back(y);
        }
        const bool sub_closed = closed[s] != 0u;
        if (sub_closed && p.size() >= 4u && std::fabs(p[0] - p[p.size()-2u]) < 1e-7f &&
            std::fabs(p[1] - p.back()) < 1e-7f) {
            smooth_points.front() &= smooth_points.back();
            smooth_points.pop_back();
            p.pop_back(); p.pop_back();
        }
        const uint32_t m = static_cast<uint32_t>(p.size() / 2u);
        if (m == 0u) continue;
        if (m == 1u) {
            // The guest may already have collapsed zero-length segments.
            const float x = p[0], y = p[1];
            if (cap == 1u) AddDisc(contours, x, y, radius, arc_segments);
            else if (cap == 2u) contours.push_back({x - radius, y - radius, x + radius, y - radius,
                                                    x + radius, y + radius, x - radius, y + radius});
            continue;
        }

        // Sweep each smooth section separately, preserving joins at authored vertices.
        const bool has_curve = m > 2u && std::any_of(
            smooth_points.begin() + 1, smooth_points.end() - 1,
            [](uint8_t value) { return value != 0; });
        if (!sub_closed && cap == 0u && join == 1u && has_curve) {
            std::vector<std::vector<float>> pieces;
            bool possible = true;
            uint32_t first_piece = 0;
            for (uint32_t end = 1; end < m; ++end) {
                if (end + 1u < m && smooth_points[end]) continue;
                std::vector<float> piece(p.begin() + first_piece * 2u, p.begin() + (end + 1u) * 2u);
                std::vector<uint8_t> flags(smooth_points.begin() + first_piece, smooth_points.begin() + end + 1u);
                if (end > first_piece + 1u && !CanSweepSmoothPath(piece, flags, false, 0u)) {
                    possible = false;
                    break;
                }
                AddSmoothPenSweep(pieces, piece, radius);
                if (end + 1u < m)
                    AddRoundJoin(pieces, p[(end - 1u) * 2u], p[(end - 1u) * 2u + 1u],
                                 p[end * 2u], p[end * 2u + 1u],
                                 p[(end + 1u) * 2u], p[(end + 1u) * 2u + 1u], radius, arc_segments);
                first_piece = end;
            }
            if (possible) {
                contours.insert(contours.end(), pieces.begin(), pieces.end());
                continue;
            }
        }

        if (CanSweepSmoothPath(p, smooth_points, sub_closed, cap)) {
            AddSmoothPenSweep(contours, p, radius);
            continue;
        }

        /* Open-subpath endpoints are caps; other vertices are joins. Round
           joins use an outer sector, while tight curve cusps and degenerate
           closed paths retain disc coverage. Segment bodies use offset quads. */
        for (uint32_t i = 0; i < m; ++i) {
            const float vx = p[i * 2u], vy = p[i * 2u + 1u];
            if (!sub_closed && (i == 0u || i + 1u == m)) {  /* end cap */
                if (cap == 1u) AddDisc(contours, vx, vy, radius, arc_segments);  /* ROUND; BUTT none */
                continue;
            }
            const uint32_t pi = (i == 0u) ? m - 1u : i - 1u;   /* neighbors wrap when closed */
            const uint32_t ni = (i + 1u == m) ? 0u : i + 1u;
            /* Flattened points approximate a smooth curve, not authored joins.
               If the pen exceeds local curvature, retain the cusp coverage. */
            const float ux = vx - p[pi * 2u], uy = vy - p[pi * 2u + 1u];
            const float wx = p[ni * 2u] - vx, wy = p[ni * 2u + 1u] - vy;
            const float lens = std::hypot(ux, uy) * std::hypot(wx, wy);
            const float dot = lens > 0.0f ? std::clamp((ux * wx + uy * wy) / lens, -1.0f, 1.0f) : 1.0f;
            const float sin_half = std::sqrt(std::max(0.0f, (1.0f - dot) * 0.5f));
            const float curvature_radius = sin_half > 1e-6f ? (std::hypot(ux, uy) + std::hypot(wx, wy)) / (4.0f * sin_half) : 1e30f;
            const bool cusp = dot < -0.999f || radius >= curvature_radius;
            if (join == 1u && !smooth_points[i] && m > 1u && dot >= -0.999f)
                AddRoundJoin(contours, p[pi * 2u], p[pi * 2u + 1u], vx, vy,
                             p[ni * 2u], p[ni * 2u + 1u], radius, arc_segments);
            else if ((smooth_points[i] && dot < -0.995f) ||
                     (join == 1u && (!smooth_points[i] || cusp)))
                AddDisc(contours, vx, vy, radius, arc_segments);
            else
                AddMiterJoin(contours, p[pi * 2u], p[pi * 2u + 1u], vx, vy,
                             p[ni * 2u], p[ni * 2u + 1u], radius, smooth_points[i] ? -0.999f : (join == 2u ? 2.0f : miter_thresh));
        }
        const uint32_t nseg = (m == 1u) ? 0u : (sub_closed ? m : m - 1u);
        for (uint32_t i = 0; i < nseg; ++i) {
            const uint32_t a = i, b = (i + 1u == m) ? 0u : i + 1u;
            float ax = p[a * 2u], ay = p[a * 2u + 1u];
            float bx = p[b * 2u], by = p[b * 2u + 1u];
            const float dx = bx - ax, dy = by - ay;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6f) continue;
            const float nx = dy / len * radius, ny = -dx / len * radius;
            if (!sub_closed && cap == 2u) {
                if (i == 0u) { ax -= dx / len * radius; ay -= dy / len * radius; }
                if (i + 1u == nseg) { bx += dx / len * radius; by += dy / len * radius; }
            }
            contours.push_back({ax + nx, ay + ny, bx + nx, by + ny,
                                bx - nx, by - ny, ax - nx, ay - ny});
        }
    }

    for (auto& contour : contours) {
        for (size_t i = 0; i < contour.size(); i += 2u) {
            const float x = contour[i], y = contour[i + 1u];
            contour[i] = a * x + b * y;
            contour[i + 1u] = c * x + d * y;
        }
    }

    /* Normalize every contour to one winding so nonzero-inside unions overlaps
       (a piece wound the other way would subtract instead). */
    for (auto& c : contours)
        if (SignedArea2(c) < 0.0f) {
            const size_t m = c.size() / 2u;
            for (size_t i = 0; i < m / 2u; ++i) {
                std::swap(c[i * 2u], c[(m - 1u - i) * 2u]);
                std::swap(c[i * 2u + 1u], c[(m - 1u - i) * 2u + 1u]);
            }
        }

    uint32_t const_px = t.argb;
    bool per_pixel = t.paint != nullptr || t.mask_pa != 0u;
    if (t.blend) {
        if (!t.color_transform && !t.generic_blend) {
            blend.ValidateBlendProg(t.prog_a);
            blend.ValidateBlendProg(t.prog_c);
        }
        per_pixel = per_pixel || t.color_transform || t.generic_blend || blend.BlendProgRefsDest(t.prog_a) ||
                    blend.BlendProgRefsDest(t.prog_c);
        if (!per_pixel) const_px = blend.BlendPixel(t, t.argb, 0u, 1.0f);
    }
    Gpu2dFillTarget stroke_target = t;
    stroke_target.even_odd = false;
    rast.FillContours(stroke_target, contours, const_px, per_pixel);
    rast.ClearPath();
}

}
