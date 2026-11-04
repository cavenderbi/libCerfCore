#include "vga_retrace.h"

#include "vga_register_file.h"
#include "../external_device_clock.h"
#include "../../core/steady_time.h"
#include "../../core/tick_scale.h"
#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

namespace {

constexpr uint8_t kSrClockingMode = 0x01;
constexpr uint8_t kSrDots8        = 0x01;
constexpr uint8_t kSrDotClockHalf = 0x08;

constexpr uint8_t kCrHTotal       = 0x00;
constexpr uint8_t kCrHRetrace     = 0x04;
constexpr uint8_t kCrHRetraceEnd  = 0x05;
constexpr uint8_t kCrVTotal       = 0x06;
constexpr uint8_t kCrOverflow     = 0x07;
constexpr uint8_t kCrVRetrace     = 0x10;
constexpr uint8_t kCrVRetraceEnd  = 0x11;
constexpr uint8_t kCrModeControl  = 0x17;
constexpr uint8_t kCrScanLineDiv2 = 0x04;
constexpr uint8_t kCrSyncEnable   = 0x80;

constexpr uint64_t kHRetraceEndMask = 0x1Fu;
constexpr uint64_t kVRetraceEndMask = 0x0Fu;

constexpr uint8_t kSt01Retrace  = 0x01;
constexpr uint8_t kSt01VRetrace = 0x08;

constexpr uint64_t kNsPerSecond = 1000000000u;
constexpr uint64_t kUsPerSecond = 1000000u;

bool InRetrace(uint64_t counter, uint64_t start, uint64_t end_low, uint64_t mask) {
    if (counter < start) return false;
    uint64_t span = (end_low - start) & mask;
    if (span == 0u) span = mask + 1u;
    return counter - start < span;
}

}

void VgaRetrace::Attach() {
    clock_ = &emu_.Get<ExternalDeviceClock>();
}

VgaRetrace::Geometry VgaRetrace::Measure(const VgaRegisterFile& regs,
                                         GuestCycleClock::Rate master_clock) {
    const uint8_t  sr1 = regs.Sequencer(kSrClockingMode);
    const uint64_t ov  = regs.Crtc(kCrOverflow);
    Geometry g;
    g.char_dots     = (sr1 & kSrDots8) ? 8u : 9u;
    g.line_dots     = (static_cast<uint64_t>(regs.Crtc(kCrHTotal)) + 5u) * g.char_dots;
    g.counter_shift = (regs.Crtc(kCrModeControl) & kCrScanLineDiv2) ? 1u : 0u;
    g.frame_lines   = ((regs.Crtc(kCrVTotal) | ((ov & 0x01u) << 8) | ((ov & 0x20u) << 4)) + 2u)
                      << g.counter_shift;
    g.dot_den       = master_clock.den * ((sr1 & kSrDotClockHalf) ? 2u : 1u);
    return g;
}

uint64_t VgaRetrace::HostFrameCount(const VgaRegisterFile& regs,
                                    GuestCycleClock::Rate master_clock) {
    const Geometry g    = Measure(regs, master_clock);
    const uint64_t dots = MulDivU64(HostSteadyMicros(), master_clock.num, g.dot_den * kUsPerSecond);
    return dots / (g.line_dots * g.frame_lines);
}

uint8_t VgaRetrace::InputStatus1(const VgaRegisterFile& regs,
                                 GuestCycleClock::Rate master_clock) const {
    if (!(regs.Crtc(kCrModeControl) & kCrSyncEnable)) return 0u;
    const Geometry g       = Measure(regs, master_clock);
    const uint64_t dots    = MulDivU64(static_cast<uint64_t>(clock_->NowNs()), master_clock.num,
                                       g.dot_den * kNsPerSecond);
    const uint64_t ov      = regs.Crtc(kCrOverflow);
    const uint64_t v_start = regs.Crtc(kCrVRetrace) | ((ov & 0x04u) << 6) | ((ov & 0x80u) << 2);
    const uint64_t dot     = dots % (g.line_dots * g.frame_lines);
    const uint64_t line    = (dot / g.line_dots) >> g.counter_shift;
    const uint64_t chr     = (dot % g.line_dots) / g.char_dots;

    const bool vertical   = InRetrace(line, v_start, regs.Crtc(kCrVRetraceEnd), kVRetraceEndMask);
    const bool horizontal = InRetrace(chr, regs.Crtc(kCrHRetrace), regs.Crtc(kCrHRetraceEnd),
                                      kHRetraceEndMask);
    if (vertical) return kSt01VRetrace | kSt01Retrace;
    return horizontal ? kSt01Retrace : 0u;
}

}
