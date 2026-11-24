#define NOMINMAX
#include "imx51_gpu2d_command_engine.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "imx51_id.h"
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/state/state_stream.h>
#include "imx51_gpu2d_rasterizer.h"
#include "imx51_gpu2d_direct2d.h"
#include "imx51_gpu2d_image_paint.h"
#include "imx51_gpu2d_stroker.h"
#include "imx51_gpu2d_vg_fill.h"
#include "imx51_gpu2d_regfile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace cerfcore {

namespace {
using namespace imx51_g2d_regfile;

/* VGV3 command-processor registers (vgregs_z160.h:212-217). */
constexpr uint32_t kAddrVgv3Control  = 0x70u;  /* render kick; CONTROL.MARKADD[11:0] = mark budget */
constexpr uint32_t kAddrVgv3Mode     = 0x71u;  /* WRITEFLUSH[2] (vgregs_z160.h:1556-1562) */
constexpr uint32_t kAddrVgv3NextAddr = 0x75u;  /* CALLADDR */
constexpr uint32_t kAddrVgv3NextCmd  = 0x76u;  /* COUNT[11:0]/op[14:12]/MARK[15]/CALLCOUNT[27:16] */
constexpr uint32_t kCtrlMarkAddMask  = 0xFFFu;  /* VGV3_CONTROL.MARKADD[11:0], vgregs_z160.h:3557-3559 */
/* Command-word top-byte opcodes (vgregs_z160.h:214/224/227). */
constexpr uint32_t kOpWriteS8  = 0x78u;  /* ADDR_VGV3_WRITES8 (lowest WRITE opcode) */
constexpr uint32_t kOpWriteRaw = 0x7Cu;  /* ADDR_VGV3_WRITERAW */
constexpr uint32_t kOpVgv3Last = 0x7Fu;  /* ADDR_VGV3_LAST pad */
constexpr uint32_t kFmtRaw = 4u;  /* WRITE RAW (no conversion), vgenums_z160.h:237 */
/* NEXTCMD walk ops (vgenums_z160.h:224-229); 3/4 "Not supported", 6/7 reserved. */
constexpr uint32_t kNextCmdContinue = 0u;
constexpr uint32_t kNextCmdJump     = 1u;
constexpr uint32_t kNextCmdCall     = 2u;
constexpr uint32_t kNextCmdAbort    = 5u;
constexpr uint32_t kMaxVgv3Packets  = 8192u;  /* walk loop guard */
/* FLUSH = a WRITE of ADDR_VGV2_ACTION = 15 (vgenums_z160.h:254). */
constexpr uint32_t kActFlush        = 15u;
constexpr uint32_t kAddrVgv2Action  = 0x6Fu;  /* ADDR_VGV2_ACTION, vgregs_z160.h:169 */
/* WRITE FORMAT lanes (vgenums_z160.h:229-238), consumed by ExecGeometry. */
constexpr uint32_t kFmtS8  = 0u;
constexpr uint32_t kFmtS16 = 1u;
constexpr uint32_t kFmtS32 = 2u;
constexpr uint32_t kFmtF32 = 3u;
/* V2_ACTION path ops (vgenums_z160.h:240-254), consumed by ExecGeometry. */
constexpr uint32_t kActMoveToOpen   = 1u;
constexpr uint32_t kActMoveToClosed = 2u;
constexpr uint32_t kActLineTo       = 3u;
constexpr uint32_t kActCubicTo      = 4u;
constexpr uint32_t kActSmoothCubicTo = 6u;
constexpr uint32_t kActSmoothQuadTo = 7u;
constexpr uint32_t kActQuadTo       = 5u;  /* VGV2_ACTION_QUADTO: quadratic C1,C3,C4 */
constexpr uint32_t kAddrC2X         = 0x42u;
constexpr uint32_t kAddrC3X         = 0x44u;  /* ADDR_VGV2_C3X (control pt; C4 follows at 0x46) */
/* VGV2 geometry/transform registers (vgregs_z160.h:178-211). */
constexpr uint32_t kAddrC4X      = 0x46u;  /* ADDR_VGV2_C4X */
constexpr uint32_t kAddrC4XRel   = 0x4Eu;  /* ADDR_VGV2_C4XREL */
constexpr uint32_t kAddrXfXX     = 0x50u;  /* ADDR_VGV2_XFXX..XFYA = 0x50-0x55 */
constexpr uint32_t kAddrVgv2Mode = 0x6Eu;  /* ADDR_VGV2_MODE: STROKE[8], EXPONENTADD[23:18] */
constexpr uint32_t kAddrVgv1Cbase1  = 0x2Au;  /* ADDR_VGV1_CBASE1, vgregs_z160.h:157 */
constexpr uint32_t kAddrVgv1Ubase2  = 0x2Bu;  /* ADDR_VGV1_UBASE2, vgregs_z160.h:165 */
constexpr uint32_t kAddrVgv1Tileofs = 0x22u;  /* ADDR_VGV1_TILEOFS: X[11:0]/Y[23:12], vgregs_z160.h:164/1277 */
constexpr uint32_t kAddrG2dInput    = 0x0Fu;  /* ADDR_G2D_INPUT, vgregs_z160.h:96 */
constexpr uint32_t kAddrG2dXy       = 0xF0u;  /* ADDR_G2D_XY: Y[11:0]/X[27:16] signed, vgregs_z160.h:105/857 */
constexpr uint32_t kAddrG2dWh       = 0xF1u;  /* ADDR_G2D_WIDTHHEIGHT: HEIGHT[11:0]/WIDTH[27:16], vgregs_z160.h:104/850 */
constexpr uint32_t kAddrG2dSxy      = 0xF2u;  /* ADDR_G2D_SXY: Y[10:0]/X[26:16] unsigned, vgregs_z160.h:828 - source-copy fire */
constexpr uint32_t kAddrG2dColor    = 0xFFu;  /* ADDR_G2D_COLOR, vgregs_z160.h:83 */

}  /* namespace */

REGISTER_SERVICE(Imx51Gpu2dCommandEngine);

bool Imx51Gpu2dCommandEngine::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetSocId() == SocId::Imx51;
}

void Imx51Gpu2dCommandEngine::Halt(const char* why, uint32_t addr, uint32_t data) const {
    emu_.Get<Fatal>().Die("[GPU2D-CMD] %s at 0x%08X (value 0x%08X)", why, addr, data);
}

bool Imx51Gpu2dCommandEngine::WriteReg(uint32_t addr, uint32_t data) {
    if (addr == kAddrVgv3Control) {
        /* Render kick (gsl_g12.c:810 = CONTROL=flags then CONTROL=0): CONTROL.MARKADD[11:0]
           is the mark budget the walk auto-pauses at; bits beyond it (vgregs_z160.h:1534)
           are unmodeled control modes. */
        if (data == 0u) return false;
        if (data & ~kCtrlMarkAddMask)
            Halt("VGV3_CONTROL bits beyond MARKADD (not modeled)", addr, data);
        return ProcessVgv3Stream(data);
    }
    StoreReg(addr, data);
    return false;
}

/* The exact registers a modeled path reads back from vg_regs_ (FlushPath +
   EmitPoint). Grounded per-register by its consumer; nothing is stored just
   because its address falls in a range. */
bool Imx51Gpu2dCommandEngine::IsConsumedConfigReg(uint32_t reg) {
    switch (reg) {
        case 0x00u:  /* G2D_BASE0 (dest)              - FlushPath t.dest_pa */
        case 0x01u:  /* G2D_CFG0 (format/stride)      - FlushPath */
        case 0x02u:  /* G2D_BASE1 (copy source)       - Direct2d::Copy src_pa */
        case 0x03u:  /* G2D_CFG1 (source format)      - Direct2d::Copy src stride/format */
        case 0x08u:  /* G2D_SCISSORX                  - FlushPath clip */
        case 0x09u:  /* G2D_SCISSORY                  - FlushPath clip */
        case 0x0Cu:  /* G2D_ALPHABLEND                - FlushPath gate */
        case 0x0Du:  /* G2D_ROP                       - FlushPath gate */
        case 0x0Eu:  /* G2D_CONFIG                    - FlushPath gate */
        case 0x0Fu:  /* G2D_INPUT                     - FlushPath gate */
        case 0x10u:  /* G2D_MASK                      - FlushPath gate */
        case 0x11u:  /* G2D_BLENDERCFG                - FlushPath blend */
        case 0x14u:  /* G2D_BLEND_A0                  - FlushPath t.prog_a / ImagePaint setup */
        case 0x18u:  /* G2D_BLEND_C0                  - FlushPath t.prog_c / ImagePaint setup */
        case 0x15u:  /* G2D_BLEND_A1                  - VG fill / ImagePaint second alpha pass */
        case 0x19u:  /* G2D_BLEND_C1                  - VG fill / ImagePaint second color pass */
        case 0xB0u: case 0xB1u: case 0xB2u: case 0xB3u:
        case 0xB4u: case 0xB5u: case 0xB6u: case 0xB7u:
                     /* G2D_CONST0-7 ARGB blend operands - VG fill / ImagePaint */
        case 0x24u:  /* VGV1_SCISSORX                 - FlushPath clip */
        case 0x25u:  /* VGV1_SCISSORY                 - FlushPath clip */
        case kAddrVgv1Tileofs:  /* 0x22 VGV1_TILEOFS sub-tile offset - EmitPoint */
        case 0x27u:  /* VGV1_CFG1 (WINDRULE)          - FlushPath t.even_odd */
        case 0x28u:  /* VGV1_CFG2 (AAMODE)            - FlushPath gate */
        case 0x50u: case 0x51u: case 0x52u: case 0x53u: case 0x54u: case 0x55u:
                     /* VGV2 XF affine (XFXX..XFYA)   - EmitPoint transform */
        case 0x5Eu:  /* VGV2_SCALE (path interp)      - FlushPath identity gate */
        case 0x5Fu:  /* VGV2_BIAS (path interp)       - FlushPath identity gate */
        case kAddrVgv2Mode:  /* 0x6E STROKE/EXPONENTADD - EmitPoint + FlushPath */
        case 0x61u:  /* VGV2_THINRADIUS               - FlushPath stroke thin-radius gate */
        case 0x65u:  /* VGV2_RADIUS                   - FlushPath stroke half-width */
        case 0x66u:  /* VGV2_MITER                    - stroker miter-limit test (join==MITER) */
        case 0xD0u:  /* G2D_GRADIENT                  - FlushPath gate / ImagePaint gate */
        case 0xC0u: case 0xC1u: case 0xC2u: case 0xC3u: case 0xC4u: case 0xC5u:
                     /* GRADW CONST0-5                - ImagePaint affine paint-matrix coeffs */
        case 0xD1u:  /* GRADW_TEXCFG                  - ImagePaint texture format/stride */
        case 0xD2u:  /* GRADW_TEXSIZE                 - ImagePaint texel dims */
        case 0xD3u:  /* GRADW_TEXBASE                 - ImagePaint texture base */
        case 0xE0u: case 0xE1u:  /* GRADW_INST0/1     - ImagePaint S/T affine MAC */
        case kAddrG2dXy:     /* 0xF0 rect origin       - Direct2d::Fill/Copy / ImagePaint */
        case kAddrG2dWh:     /* 0xF1 rect dimensions   - Direct2d::Fill/Copy / ImagePaint */
            return true;
        default:
            return false;
    }
}

void Imx51Gpu2dCommandEngine::StoreReg(uint32_t reg, uint32_t data) {
    if (reg == 0xD0u && data == 0u) paint_bank_valid_ = false;
    if (reg == 0xD0u && (vg_regs_[0xD0] & ~7u) == 0x40u && (data & 0x1C0u) == 0x1C0u) {
        std::copy_n(vg_regs_, 0x100, paint_regs_);
        paint_bank_valid_ = true;
    }
    switch (reg) {
        case kAddrVgv3Control:
            /* The command stream can flush the blit cache; writes are synchronous here. */
            if (data == (1u << 19)) return;
            Halt("in-stream VGV3_CONTROL mode (not modeled)", reg, data);
        case kAddrVgv3NextAddr: vgv3_nextaddr_ = data; return;  /* CALLADDR, consumed by the walk */
        case kAddrVgv3NextCmd:  vgv3_nextcmd_ = data; return;   /* NEXTCMD, consumed by the walk */
        case kAddrVgv3Mode:
            /* VGV3_MODE=4 = WRITEFLUSH (vgregs_z160.h:1559); CERF applies command-stream
               register writes synchronously in StoreReg so it has no modeled effect. Other
               MODE bits (FLIPENDIAN/DMIPAUSETYPE/DMIRESET) are unmodeled. */
            if (data == 4u) return;
            Halt("VGV3_MODE bits beyond WRITEFLUSH (not modeled)", reg, data);
        case kAddrVgv1Cbase1: case kAddrVgv1Ubase2:
            /* g12 EDGE0/EDGE1 HW edge-scratch bases (gsl_g12.c:60-61,869-875);
               the analytic contour-coverage rasterizer replaces the HW edge walk,
               so no CERF path reads them - proven-inert, stored for save/restore. */
            vg_regs_[reg] = data;
            return;
        case 0x04u: case 0x05u: case 0x06u: case 0x07u:
            /* G2D_BASE2/CFG2 provide the mask surface for vector fills. */
            vg_regs_[reg] = data;
            return;
        case 0x16u: case 0x17u: case 0x1Au: case 0x1Bu:
        case 0x1Cu: case 0x1Du: case 0x1Eu: case 0x1Fu:
            /* BLEND_A2/A3 + BLEND_C2-C7 = the 3rd+ blend-program passes
               (vgregs_z160.h:69-70,73-78); the composite gates BCFG=0x69 (2 passes:
               A0/A1/C0/C1) and VG-fill allows at most two passes, so these higher passes
               are never read - proven-inert, stored. */
            vg_regs_[reg] = data;
            return;
        case 0x0Au: case 0x0Bu:
            /* G2D_FOREGROUND/BACKGROUND provide colors for one-bit textures. */
            vg_regs_[reg] = data;
            return;
        case 0x60u: case 0x62u: case 0x63u: case 0x64u:
            /* VGV2_ACCURACY + ARCCOS/ARCSIN/ARCTAN (vgregs_z160.h:168,170-172) = HW
               round-arc tessellation-tolerance hints the stroke setup (sub_41C63970/
               sub_41C5B1B8) writes per stroke; the stroker uses fixed 16-facet
               analytic discs and reads none - proven-inert, stored. */
            vg_regs_[reg] = data;
            return;
        case 0x68u:
            /* VGV2_CLIP (vgregs_z160.h:194) = a single-float HW coordinate guard-band
               = scale*2048 (sub_41C63970), not a user clip rect; CERF clips the fill to
               the scissor (0x08/0x09/0x24/0x25), which sits inside the guard band, so
               the HW coordinate clip is subsumed - proven-inert, stored. */
            vg_regs_[reg] = data;
            return;
        case 0x56u: case 0x57u: case 0x58u: case 0x59u:
            /* VGV2_XFST (vgregs_z160.h:202-205) = the HW inverse-XF (device->user)
               transform (sub_41C63970 = the negated forward-XF inverse); CERF uses the
               forward XF (0x50-0x55) for geometry and an independent device->texel GRADW
               CONST for paint, so the XFST inverse is redundant - proven-inert. */
            vg_regs_[reg] = data;
            return;
        case 0xE2u: case 0xE3u: case 0xE4u: case 0xE5u: case 0xE6u: case 0xE7u:
            /* GRADW_INST2-7 = ISA slots past the modeled 2-instruction affine (INST0/1);
               a radial/multi-instruction gradient writes them but has a non-matching
               INST0 that FATALs at the image_paint/vg_fill INST0 gate, so INST2-7 stay
               0 for every gradient that renders - proven-inert, stored. */
            vg_regs_[reg] = data;
            return;
        case 0xC6u: case 0xC7u: case 0xC8u: case 0xC9u: case 0xCAu: case 0xCBu:
            /* GRADW_CONST6-11 (vgregs_z160.h:113-118): the modeled program (INST0/INST1
               exact-match gate) references only GRADREG C0-C5 and SampleGradw reads only
               CONST0-5; a CONST6-11 program writes the radial INST0=0x04080632 that FATALs
               at that gate, so CONST6-11 are never read by a rendering paint - proven-inert. */
            vg_regs_[reg] = data;
            return;
        case 0xD4u:
            /* GRADW_BORDERCOLOR is consumed by the border wrap mode. */
            vg_regs_[reg] = data;
            return;
        case 0x23u:
            /* VGV1_FILL.INHERIT (vgregs_z160.h:161) = the AA tile loop's per-tile HW
               edge-buffer chaining hint (sub_41C62758 writes reg 35 to chain coverage
               across a split fill's tiles); CERF's analytic per-FLUSH rasterizer recomputes
               each tile with no edge buffer to inherit - proven-inert, stored. */
            vg_regs_[reg] = data;
            return;
        case 0x5Au: case 0x5Bu: case 0x5Cu: case 0x5Du:
            /* VGV2_BBOX MINX/MINY/MAXX/MAXY (vgregs_z160.h:173-176): the tile
               loops (sub_41C5B468 / sub_41C62758) write the band/tile rect with
               guard margins as (bound_px -/+ margin)*2^-exp - XFT - consumed by
               the FLUSH superset gate. */
            vg_regs_[reg] = data;
            bbox_live_ = true;
            return;
        case kAddrVgv2Action:
            if (data == 0u) return;                    /* VGV2_ACTION_END */
            if (data == kActFlush) { FlushPath(); return; }
            Halt("VGV2_ACTION register value (not modeled)", reg, data);
        case 0xFEu:
            /* G2D_IDLE IRQ|BCFLUSH (vgregs_z160.h:789), csi_stream_flush's per-submit
               trigger. Raising an IRQ/count here would double-count the submit: the
               guest budgets 1 per flush (issueibcmds current_timestamp++), which
               CompleteSubmit already delivers at the walk terminator. */
            if (data == 3u) return;
            Halt("G2D_IDLE mode (not modeled)", reg, data);
        case kAddrG2dSxy:
            /* SXY fires the source->dest copy under INPUT=0 (sub_41C6B2FC) or
               INPUT=2 (sub_41C6C448); both write SXY last as the trigger. */
            if (vg_regs_[kAddrG2dInput] == 0u || vg_regs_[kAddrG2dInput] == 2u) {
                emu_.Get<Imx51Gpu2dDirect2d>().Copy(vg_regs_, data);
                return;
            }
            vg_regs_[reg] = data;
            return;
        case kAddrG2dColor:
            /* COLOR is the direct-2D engine's per-rect latch: the rect emitter
               (sub_41C620DC) rewrites XY/WIDTHHEIGHT/COLOR per 1024px tile,
               COLOR last - an earlier latch would pair a new rect with a stale
               color. Under INPUT=0x11 (VG fill) it is paint only (FLUSH reads it). */
            vg_regs_[kAddrG2dColor] = data;
            /* VG-mode paint latches, NOT fire triggers: 0x11 (fill emit) and
               0x19 (sub_41C60610 preamble writes INPUT <- static 0x19 then COLOR
               mid-config with stale XY/WH); the fill itself fires at FLUSH. */
            if (vg_regs_[kAddrG2dInput] == 0x11u || vg_regs_[kAddrG2dInput] == 0x19u)
                return;
            /* INPUT 0x01 (sub_41C6C6DC), 0x09 and 0x1B (libOpenVG.dll VA 0x41C718B8)
               are rect fills; under 0x09 with GRADIENT.ENABLE2 + the blender enabled
               it is the image-paint composite (sub_41C6338C) instead. */
            if (vg_regs_[kAddrG2dInput] == 0x01u || vg_regs_[kAddrG2dInput] == 0x09u ||
                vg_regs_[kAddrG2dInput] == 0x1Bu) {
                if ((vg_regs_[0xD0] & 0x80u) && (vg_regs_[0x11] & (1u << 5))) {
                    emu_.Get<Imx51Gpu2dImagePaint>().Composite(vg_regs_);
                    return;
                }
                emu_.Get<Imx51Gpu2dDirect2d>().Fill(vg_regs_);
                return;
            }
            Halt("G2D_COLOR write under unmodeled G2D_INPUT", reg, vg_regs_[kAddrG2dInput]);
    }
    if (IsConsumedConfigReg(reg)) {
        vg_regs_[reg] = data;
        if (reg == kAddrG2dWh && vg_regs_[kAddrG2dInput] == 0x08u &&
            ((vg_regs_[0xD0] & ~7u) == 0x40u || (vg_regs_[0xD0] & 0x1C0u) == 0x1C0u))
            emu_.Get<Imx51Gpu2dImagePaint>().Composite(vg_regs_, paint_bank_valid_ ? paint_regs_ : nullptr);
        return;
    }
    Halt("g12 register write has no modeled consumer", reg, data);
}

uint32_t Imx51Gpu2dCommandEngine::ReadPa(uint32_t pa) {
    const uint8_t* hp = emu_.Get<EmulatedMemory>().TryTranslate(pa);
    if (!hp) Halt("VGV3 stream read unbacked", pa, 0);
    return *reinterpret_cast<const uint32_t*>(hp);
}

/* Walk the g12 command stream from the persistent cursor (gpuaddr==physical), pausing
   when the kick's MARKADD count of NEXTCMD.MARK[15] advances is reached - the grounded
   HW auto-pause (vgenums_z160.h:224-229, csi_stream_endpacket), never a no-rearm guess
   (that over-reads an un-prepared slot's zero tail). Retires on the budget or ABORT. */
bool Imx51Gpu2dCommandEngine::ProcessVgv3Stream(uint32_t mark_budget) {
    uint32_t marks = 0;
    for (uint32_t guard = 0; guard < kMaxVgv3Packets; ++guard) {
        const uint32_t nextcmd = vgv3_nextcmd_;
        const uint32_t op = (nextcmd >> 12) & 7u, count = nextcmd & 0xFFFu,
                       callcount = (nextcmd >> 16) & 0xFFFu;
        const bool mark = ((nextcmd >> 15) & 1u) != 0u;
        switch (op) {
            case kNextCmdAbort:    return true;
            /* CALL runs the sub-stream at CALLADDR for CALLCOUNT dwords then a continue
               (vgenums_z160.h:226); the sub-stream's re-arm is discarded. */
            case kNextCmdCall:     ExecPacket(vgv3_nextaddr_, callcount); break;
            case kNextCmdJump:     vgv3_cursor_ = vgv3_nextaddr_; break;
            case kNextCmdContinue: break;
            default: Halt("VGV3 NEXTCMD op reserved/unsupported", vgv3_cursor_, nextcmd);
        }
        ExecPacket(vgv3_cursor_, count);  /* count dwords; a WRITE to 0x75/0x76 re-arms */
        vgv3_cursor_ += count * 4u;
        if (mark && ++marks == mark_budget) return true;
    }
    Halt("VGV3 walk exceeded guard", vgv3_cursor_, vgv3_nextcmd_);
}

/* One packet of `count` dwords: WRITE opcode[31:24]/ACTION[23:20]/LOOP[19:16]/
   COUNT[15:8]/ADDR[7:0] + data dwords, or DIRECT value[23:0]|reg[31:24], or LAST
   pad (csi_stream_regwrite sub_41C96004 + path emitter dispatch sub_41C7772C). */
void Imx51Gpu2dCommandEngine::ExecPacket(uint32_t base, uint32_t count) {
    for (uint32_t i = 0; i < count;) {
        const uint32_t w = ReadPa(base + i * 4u), top = w >> 24;
        if (top == kOpVgv3Last) { ++i; continue; }
        if (top >= kOpWriteS8 && top <= kOpWriteRaw) {
            const uint32_t a = w & 0xFFu, cnt = (w >> 8) & 0xFFu, loop = (w >> 16) & 0xFu,
                           action = (w >> 20) & 0xFu, fmt = top - kOpWriteS8;
            if (action != 0u) {
                i += 1u + ExecGeometry(action, a, fmt, cnt, loop, base + (i + 1u) * 4u);
                continue;
            }
            if (fmt != kFmtF32 && fmt != kFmtRaw)
                Halt("VGV3 config WRITE S8/S16/S32 (not modeled)", base + i * 4u, w);
            for (uint32_t k = 0; k < cnt; ++k)  /* F32/RAW: one data dword per register */
                StoreReg(a + k, ReadPa(base + (i + 1u + k) * 4u));
            i += 1u + cnt;
            continue;
        }
        StoreReg(top, w & 0xFFFFFFu);  /* DIRECT */
        ++i;
    }
}

/* Geometry values pack per V3_FORMAT lanes (vgenums_z160.h:229-238: S8 x4,
   S16 x2, S32|F32 x1 per dword, int lanes -> float) and LOOP=2 values per
   ACTION repetition (emitters sub_41C7712C/76EB8/774C8/76C5C) - NOT one
   value per consecutive register like config WRITEs. Returns data dwords. */
uint32_t Imx51Gpu2dCommandEngine::ExecGeometry(uint32_t action, uint32_t addr, uint32_t fmt,
                                               uint32_t cnt, uint32_t loop, uint32_t data_pa) {
    const bool quad = (action == kActQuadTo);
    const bool cubic = (action == kActCubicTo);
    const bool smooth_cubic = (action == kActSmoothCubicTo);
    const bool smooth_quad = (action == kActSmoothQuadTo);
    if (action != kActMoveToOpen && action != kActMoveToClosed && action != kActLineTo && !quad && !smooth_quad && !cubic && !smooth_cubic) {
        Halt("VGV2 curve/vertex action (not modeled)", data_pa, action);
    }
    const bool rel = (addr == kAddrC4XRel);
    const bool cubic_rel = addr == kAddrC2X + 8u;
    const bool quad_rel = addr == kAddrC3X + 8u;
    if (cubic) {
        if (loop != 6u || cnt == 0u || cnt % 6u != 0u || (addr != kAddrC2X && !cubic_rel))
            Halt("VGV2 CUBICTO target/loop/count", data_pa, (loop << 16) | cnt);
    } else if (quad || smooth_cubic) {
        /* QUADTO writes C3(control 0x44/0x45) then C4(end 0x46/0x47): LOOP=4, count a
           multiple of 4, absolute C3X target (captured shape, encoder sub_41C7772C). */
        if (loop != 4u || cnt == 0u || (cnt & 3u) != 0u)
            Halt("VGV2 QUADTO loop/count shape", data_pa, (loop << 16) | cnt);
        if (addr != kAddrC3X && !quad_rel)
            Halt("VGV2 QUADTO target register", data_pa, addr);
    } else {
        if (loop != 2u || cnt == 0u || (cnt & 1u) != 0u)
            Halt("VGV2 geometry loop/count shape", data_pa, (loop << 16) | cnt);
        if (addr != kAddrC4X && !(addr == kAddrC2X && action == kActLineTo) &&
            !(rel && (action == kActLineTo || smooth_quad)))
            Halt("VGV2 geometry target register", data_pa, addr);
    }
    uint32_t lanes = 1u, word = 0;
    if (fmt == kFmtS8) lanes = 4u;
    else if (fmt == kFmtS16) lanes = 2u;
    else if (fmt != kFmtS32 && fmt != kFmtF32)
        Halt("VGV2 geometry RAW format", data_pa, fmt);
    float pt[6];
    for (uint32_t k = 0; k < cnt; ++k) {
        const uint32_t lane = k % lanes;
        if (lane == 0u) word = ReadPa(data_pa + (k / lanes) * 4u);
        float v;
        switch (fmt) {
            case kFmtS8:  v = static_cast<float>(static_cast<int8_t>(word >> (lane * 8u))); break;
            case kFmtS16: v = static_cast<float>(static_cast<int16_t>(word >> (lane * 16u))); break;
            case kFmtS32: v = static_cast<float>(static_cast<int32_t>(word)); break;
            default:      std::memcpy(&v, &word, sizeof(v)); break;  /* F32 */
        }
        if (cubic) {
            pt[k % 6u] = v;
            if (k % 6u != 5u) continue;
            const float ox = cubic_rel ? cur_x_ : 0.0f, oy = cubic_rel ? cur_y_ : 0.0f;
            EmitCubic(pt[0] + ox, pt[1] + oy, pt[2] + ox, pt[3] + oy, pt[4] + ox, pt[5] + oy);
        } else if (quad || smooth_cubic) {
            pt[k & 3u] = v;                       /* C3x,C3y,C4x,C4y (absolute) */
            if ((k & 3u) != 3u) continue;
            if (quad_rel) {
                pt[0] += cur_x_; pt[1] += cur_y_; pt[2] += cur_x_; pt[3] += cur_y_;
            }
            if (smooth_cubic) {
                const float c2x = cubic_control_valid_ ? 2.0f * cur_x_ - cubic_control_x_ : cur_x_;
                const float c2y = cubic_control_valid_ ? 2.0f * cur_y_ - cubic_control_y_ : cur_y_;
                EmitCubic(c2x, c2y, pt[0], pt[1], pt[2], pt[3]);
            } else EmitQuad(pt[0], pt[1], pt[2], pt[3]);
        } else {
            pt[k & 1u] = v;
            if ((k & 1u) == 0u) continue;
            if (smooth_quad) {
                const float control_x = quad_control_valid_ ? 2.0f * cur_x_ - quad_control_x_ : cur_x_;
                const float control_y = quad_control_valid_ ? 2.0f * cur_y_ - quad_control_y_ : cur_y_;
                EmitQuad(control_x, control_y, rel ? cur_x_ + pt[0] : pt[0],
                         rel ? cur_y_ + pt[1] : pt[1]);
            } else {
                if (addr == kAddrC2X && (pt[0] != cur_x_ || pt[1] != cur_y_))
                    Halt("VGV2 LINETO C2 differs from current point", data_pa, addr);
                if (rel) { cur_x_ += pt[0]; cur_y_ += pt[1]; }
                else     { cur_x_ = pt[0];  cur_y_ = pt[1]; }
                quad_control_valid_ = false;
                cubic_control_valid_ = false;
                EmitPoint(action);
            }
        }
    }
    return (cnt + lanes - 1u) / lanes;
}

/* Path->device: XF affine (0x50-0x55) * 2^-exp (VGV2_MODE.EXPONENTADD, sub_41C63970)
   + TILEOFS.X/Y[5:0] rebase (sub_41C5B468). Dropping 2^exp => wrong power-of-two scale;
   dropping TILEOFS => up to 63px shift on a non-64-aligned fill. */
void Imx51Gpu2dCommandEngine::DevicePoint(float px, float py, float& dx, float& dy) const {
    const float sx = RegF(vg_regs_, kAddrXfXX) * px + RegF(vg_regs_, kAddrXfXX + 2u) * py
                   + RegF(vg_regs_, kAddrXfXX + 4u);
    const float sy = RegF(vg_regs_, kAddrXfXX + 1u) * px + RegF(vg_regs_, kAddrXfXX + 3u) * py
                   + RegF(vg_regs_, kAddrXfXX + 5u);
    const float k = DeviceScale(vg_regs_);
    const uint32_t tof = vg_regs_[kAddrVgv1Tileofs];
    dx = sx * k + static_cast<float>(tof & 0x3Fu);
    dy = sy * k + static_cast<float>((tof >> 12) & 0x3Fu);
}

void Imx51Gpu2dCommandEngine::EmitPoint(uint32_t action) {
    float dx, dy;
    DevicePoint(cur_x_, cur_y_, dx, dy);
    auto& rast = emu_.Get<Imx51Gpu2dRasterizer>();
    if (action == kActLineTo) rast.LineTo(dx, dy);
    else rast.MoveTo(dx, dy, action == kActMoveToClosed);
}

/* Flatten C1(current)->C3->C4 to device LineTos, subdivided to <=1/16 device px
   (g12 VGSPAN coverage resolution): quadratic error <=|d1-2d3+d4|/(4n^2) -> n=ceil(2*sqrt|dd|). */
void Imx51Gpu2dCommandEngine::EmitQuad(float c3x, float c3y, float c4x, float c4y) {
    const float c1x = cur_x_, c1y = cur_y_;
    float d1x, d1y, d3x, d3y, d4x, d4y;
    DevicePoint(c1x, c1y, d1x, d1y);
    DevicePoint(c3x, c3y, d3x, d3y);
    DevicePoint(c4x, c4y, d4x, d4y);
    const float ddx = d1x - 2.0f * d3x + d4x, ddy = d1y - 2.0f * d3y + d4y;
    const float dev = std::sqrt(ddx * ddx + ddy * ddy);
    int n = dev > 0.0f ? static_cast<int>(std::ceil(2.0f * std::sqrt(dev))) : 1;
    n = std::clamp(n, 1, 256);
    auto& rast = emu_.Get<Imx51Gpu2dRasterizer>();
    for (int i = 1; i <= n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n), u = 1.0f - t;
        const float px = c1x + 2.0f * u * t * (c3x - c1x) + t * t * (c4x - c1x);
        const float py = c1y + 2.0f * u * t * (c3y - c1y) + t * t * (c4y - c1y);
        float dx, dy;
        DevicePoint(px, py, dx, dy);
        rast.LineTo(dx, dy, i != n);
    }
    cur_x_ = c4x;
    cur_y_ = c4y;
    quad_control_x_ = c3x;
    quad_control_y_ = c3y;
    quad_control_valid_ = true;
    // VGV2 smooth actions reflect the last control point across curve types.
    cubic_control_x_ = c3x;
    cubic_control_y_ = c3y;
    cubic_control_valid_ = true;
}

void Imx51Gpu2dCommandEngine::EmitCubic(float c2x, float c2y, float c3x, float c3y,
                                       float c4x, float c4y) {
    const float c1x = cur_x_, c1y = cur_y_;
    float dx[4], dy[4];
    DevicePoint(c1x, c1y, dx[0], dy[0]); DevicePoint(c2x, c2y, dx[1], dy[1]);
    DevicePoint(c3x, c3y, dx[2], dy[2]); DevicePoint(c4x, c4y, dx[3], dy[3]);
    const float a = std::hypot(dx[0] - 2.0f * dx[1] + dx[2], dy[0] - 2.0f * dy[1] + dy[2]);
    const float b = std::hypot(dx[1] - 2.0f * dx[2] + dx[3], dy[1] - 2.0f * dy[2] + dy[3]);
    // A tiny centerline can still have a wide stroke: retain its changing tangent.
    constexpr int kMinCubicSegments = 32;
    const int count = std::clamp(static_cast<int>(std::ceil(std::sqrt(192.0f * std::max(a, b)))),
                                 kMinCubicSegments, 256);
    auto& rast = emu_.Get<Imx51Gpu2dRasterizer>();
    // The offset pen can turn sharply even when the centerline barely moves.
    // Subdivide the original intervals where the cubic tangent changes rapidly.
    auto tangent = [&](float t) {
        const float u = 1.0f - t;
        return std::pair<float, float>{u*u*(c2x-c1x) + 2*u*t*(c3x-c2x) + t*t*(c4x-c3x),
                                       u*u*(c2y-c1y) + 2*u*t*(c3y-c2y) + t*t*(c4y-c3y)};
    };
    // Very small curves are sensitive to rounding when their offset pen turns.
    constexpr float kSubpixelExtent = 1.0f / 16.0f;
    const float curve_extent = std::max({std::hypot(dx[1] - dx[0], dy[1] - dy[0]),
                                         std::hypot(dx[2] - dx[0], dy[2] - dy[0]),
                                         std::hypot(dx[3] - dx[0], dy[3] - dy[0])});
    const float max_turn_cosine = curve_extent <= kSubpixelExtent ? 0.999f : 0.9995f;
    auto turns_sharply = [&](float left, float right) {
        const auto a = tangent(left), b = tangent(right);
        const float a_len = std::hypot(a.first, a.second), b_len = std::hypot(b.first, b.second);
        constexpr float kMinTangentLength = 1e-9f;
        return a_len > kMinTangentLength && b_len > kMinTangentLength &&
            a.first*b.first + a.second*b.second < max_turn_cosine*a_len*b_len;
    };
    // Preserve endpoint tangent directions when float precision permits it.
    const float endpoint_t = 0.25f / (count * count);
    auto endpoint_has_precision = [&](int endpoint, int control) {
        float magnitude = std::max({1.0f, std::fabs(dx[endpoint]), std::fabs(dy[endpoint])});
        const float xx = RegF(vg_regs_, kAddrXfXX), xy = RegF(vg_regs_, kAddrXfXX + 2u);
        const float yx = RegF(vg_regs_, kAddrXfXX + 1u), yy = RegF(vg_regs_, kAddrXfXX + 3u);
        const float determinant = std::fabs(xx * yy - xy * yx);
        const float condition = determinant > 1e-12f
            ? std::max(std::fabs(xx) + std::fabs(xy), std::fabs(yx) + std::fabs(yy)) *
              std::max(std::fabs(yy) + std::fabs(xy), std::fabs(yx) + std::fabs(xx)) / determinant
            : 1e20f;
        magnitude *= condition;
        const float displacement = 3.0f * endpoint_t *
            std::hypot(dx[control] - dx[endpoint], dy[control] - dy[endpoint]);
        return displacement > 32.0f * std::numeric_limits<float>::epsilon() * magnitude;
    };
    auto emit_endpoint_sample = [&](float t) {
        const float u = 1.0f - t;
        const float px = c1x + 3*u*u*t*(c2x-c1x) + 3*u*t*t*(c3x-c1x) + t*t*t*(c4x-c1x);
        const float py = c1y + 3*u*u*t*(c2y-c1y) + 3*u*t*t*(c3y-c1y) + t*t*t*(c4y-c1y);
        float x, y;
        DevicePoint(px, py, x, y);
        rast.LineTo(x, y, true);
    };
    if (endpoint_has_precision(0, 1)) emit_endpoint_sample(endpoint_t);
    auto emit_interval = [&](auto&& self, float left, float right, int depth) -> void {
        const float mid = (left + right) * 0.5f;
        constexpr int kMaxTurnSubdivision = 7;
        if (depth < kMaxTurnSubdivision && (turns_sharply(left, mid) || turns_sharply(mid, right))) {
            self(self, left, mid, depth + 1);
            self(self, mid, right, depth + 1);
            return;
        }
        if (right == 1.0f && left < 1.0f - endpoint_t && endpoint_has_precision(3, 2))
            emit_endpoint_sample(1.0f - endpoint_t);
        const float t = right, u = 1.0f - t;
        const float px = c1x + 3*u*u*t*(c2x-c1x) + 3*u*t*t*(c3x-c1x) + t*t*t*(c4x-c1x);
        const float py = c1y + 3*u*u*t*(c2y-c1y) + 3*u*t*t*(c3y-c1y) + t*t*t*(c4y-c1y);
        float x, y;
        DevicePoint(px, py, x, y);
        rast.LineTo(x, y, right < 1.0f);
    };
    for (int i = 1; i <= count; ++i)
        emit_interval(emit_interval, static_cast<float>(i - 1) / count, static_cast<float>(i) / count, 0);
    cur_x_ = c4x; cur_y_ = c4y;
    quad_control_x_ = c3x;
    quad_control_y_ = c3y;
    quad_control_valid_ = true;
    cubic_control_x_ = c3x; cubic_control_y_ = c3y; cubic_control_valid_ = true;
}

/* FLUSH (VGV2_ACTION=15): fill/stroke the accumulated path. The gate + target
   build + rasterizer/stroker dispatch live in the VG-fill service. */
void Imx51Gpu2dCommandEngine::FlushPath() {
    emu_.Get<Imx51Gpu2dVgFill>().Flush(vg_regs_, bbox_live_, paint_bank_valid_ ? paint_regs_ : nullptr);
}

void Imx51Gpu2dCommandEngine::SaveState(StateWriter& w) const {
    w.Write("vgv3_nextaddr", vgv3_nextaddr_);
    w.Write("vgv3_nextcmd", vgv3_nextcmd_);
    w.Write("vgv3_cursor", vgv3_cursor_);
    w.Write("cur_x", cur_x_);
    w.Write("cur_y", cur_y_);
    w.Write("bbox_live", bbox_live_);
    w.WriteBytes("vg_regs", vg_regs_, sizeof(vg_regs_));
}

void Imx51Gpu2dCommandEngine::RestoreState(StateReader& r) {
    r.Read("vgv3_nextaddr", vgv3_nextaddr_);
    r.Read("vgv3_nextcmd", vgv3_nextcmd_);
    r.Read("vgv3_cursor", vgv3_cursor_);
    r.Read("cur_x", cur_x_);
    r.Read("cur_y", cur_y_);
    r.Read("bbox_live", bbox_live_);
    r.ReadBytes("vg_regs", vg_regs_, sizeof(vg_regs_));
}

void Imx51Gpu2dCommandEngine::SavePaintState(StateWriter& w) const {
    w.Write("paint_bank_valid", paint_bank_valid_);
    w.WriteBytes("paint_regs", paint_regs_, sizeof(paint_regs_));
    w.Write("quad_control_valid", quad_control_valid_);
    w.Write("quad_control_x", quad_control_x_);
    w.Write("quad_control_y", quad_control_y_);
    w.Write("cubic_control_valid", cubic_control_valid_);
    w.Write("cubic_control_x", cubic_control_x_);
    w.Write("cubic_control_y", cubic_control_y_);
}

void Imx51Gpu2dCommandEngine::RestorePaintState(StateReader& r) {
    quad_control_valid_ = false;
    quad_control_x_ = quad_control_y_ = 0.0f;
    cubic_control_valid_ = false;
    cubic_control_x_ = cubic_control_y_ = 0.0f;
    if (r.Remaining() == 0) {
        paint_bank_valid_ = false;
        std::fill(std::begin(paint_regs_), std::end(paint_regs_), 0u);
        return;
    }
    r.Read("paint_bank_valid", paint_bank_valid_);
    r.ReadBytes("paint_regs", paint_regs_, sizeof(paint_regs_));
    if (r.Remaining() != 0) {
        r.Read("quad_control_valid", quad_control_valid_);
        r.Read("quad_control_x", quad_control_x_);
        r.Read("quad_control_y", quad_control_y_);
    }
    if (r.Remaining() != 0) {
        r.Read("cubic_control_valid", cubic_control_valid_);
        r.Read("cubic_control_x", cubic_control_x_);
        r.Read("cubic_control_y", cubic_control_y_);
    }
}

void Imx51Gpu2dCommandEngine::PowerOnReset() {
    vgv3_nextaddr_ = vgv3_nextcmd_ = vgv3_cursor_ = 0;
    cur_x_ = cur_y_ = 0.0f;
    quad_control_valid_ = false;
    quad_control_x_ = quad_control_y_ = 0.0f;
    cubic_control_valid_ = false;
    cubic_control_x_ = cubic_control_y_ = 0.0f;
    bbox_live_ = false;
    paint_bank_valid_ = false;
    std::fill(std::begin(paint_regs_), std::end(paint_regs_), 0u);
    std::fill(std::begin(vg_regs_), std::end(vg_regs_), 0u);
}

}
