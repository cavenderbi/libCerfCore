#pragma once

#include <cstdint>

namespace cerfcore {

namespace cerf_vr41xx_piu_detail {

uint16_t StateAfterCntWrite(uint16_t state, uint16_t old_cfg, uint16_t new_cfg, uint16_t ascn);

bool PenChangeDetected(uint16_t state, bool pen_down);

bool CntStrobeProhibited(uint16_t state, uint16_t old_cfg, uint16_t new_cfg);

bool IntervalScanStopped(uint16_t state, uint16_t old_cfg, uint16_t new_cfg);

bool ScanKeptInState(uint16_t scan_kind, uint16_t state);

uint16_t DetectingState(uint16_t state, uint16_t scan_kind);

}

}
