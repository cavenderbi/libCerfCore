#include "vr41xx_piu_state_table.h"

#include "vr41xx_piu_regs.h"
#include "vr41xx_piu_scan_timing.h"

namespace cerfcore {

namespace cerf_vr41xx_piu_detail {

bool PenChangeDetected(uint16_t state, bool pen_down) {
    return pen_down ? state == kStWaitPenTouch : state == kStIntervalNext;
}

bool IntervalScanStopped(uint16_t state, uint16_t old_cfg, uint16_t new_cfg) {
    return state == kStIntervalNext && (new_cfg & kScanStop) && !(old_cfg & kScanStop);
}

uint16_t DetectingState(uint16_t state, uint16_t scan_kind) {
    if (state == kStPenDataScan && scan_kind == Vr41xxPiuScanTiming::kIdle) return kStIntervalNext;
    if (state == kStWaitPenTouch && scan_kind == Vr41xxPiuScanTiming::kAdPort) return kStAdPortScan;
    return state;
}

bool ScanKeptInState(uint16_t scan_kind, uint16_t state) {
    switch (scan_kind) {
        case Vr41xxPiuScanTiming::kData:   return state == kStPenDataScan;
        case Vr41xxPiuScanTiming::kCmd:    return state == kStCmdScan;
        case Vr41xxPiuScanTiming::kAdPort:
            return state == kStAdPortScan || state == kStWaitPenTouch || state == kStIntervalNext;
        default:                           return true;
    }
}

bool CntStrobeProhibited(uint16_t state, uint16_t old_cfg, uint16_t new_cfg) {
    const bool start0 = (old_cfg & kScanStart) != 0, start1 = (new_cfg & kScanStart) != 0;
    const bool stop_rise = (new_cfg & kScanStop) && !(old_cfg & kScanStop);
    const bool fires = state == kStStandby && start1 && (new_cfg & kSeqEn) &&
                       (!start0 || !(old_cfg & kSeqEn));
    if (start0 != start1 && state != kStStandby) return true;
    if (fires && ((new_cfg >> 3) & 0x3u) != 0u) return true;
    return stop_rise && (state == kStDisable || state == kStWaitPenTouch);
}

uint16_t StateAfterCntWrite(uint16_t state, uint16_t old_cfg, uint16_t new_cfg, uint16_t ascn) {
    const uint16_t mode = (new_cfg >> 3) & 0x3u;

    if (state == kStStandby && (new_cfg & kScanStart) && (new_cfg & kSeqEn) && (new_cfg & kPiuPwr) &&
        (!(old_cfg & kScanStart) || !(old_cfg & kSeqEn))) {
        return kStPenDataScan;
    }
    if (IntervalScanStopped(state, old_cfg, new_cfg)) return kStStandby;

    const bool pwr0 = (old_cfg & kPiuPwr) != 0, pwr1 = (new_cfg & kPiuPwr) != 0;
    if (!pwr0 && pwr1 && state == kStDisable) state = kStStandby;
    else if (pwr0 && !pwr1)                   state = kStDisable;

    const bool seq0 = (old_cfg & kSeqEn) != 0, seq1 = (new_cfg & kSeqEn) != 0;
    if (!seq0 && seq1 && state == kStStandby) {
        /* Standby -> ADPortScan on "PIUSeqEN = 1 & ADPSStart = 1"; the scan completes and
           the state returns to the pre-scan Standby (VR4121 UM Figure 20-4 + 20.2 (3),
           VR4102 UM Figure 19-4 + 19.2). */
        if (ascn & kAdpsStart) {
            state = kStAdPortScan;
        } else {
            state = (mode == 1) ? kStCmdScan : kStWaitPenTouch;
        }
    } else if (seq0 && !seq1 && state != kStDisable) {
        state = kStStandby;
    }

    /* nec_mobilepro_700_ce2 touch.dll sub_15A04A8 flips PIUMODE from command back to
       coordinate with PIUSEQEN still set. */
    if (((old_cfg >> 3) & 0x3u) != mode && seq0 && seq1 && state != kStDisable &&
        state != kStStandby) {
        state = (mode == 1) ? kStCmdScan : kStWaitPenTouch;
    }
    return state;
}

}

}
