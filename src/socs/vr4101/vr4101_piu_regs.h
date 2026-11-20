#pragma once

#include <cstdint>

namespace cerfcore::vr4101_piu {

inline constexpr uint16_t kPadRst     = 1u << 0;
inline constexpr uint16_t kPiuPwr     = 1u << 1;
inline constexpr uint16_t kSeqEn      = 1u << 2;
inline constexpr uint16_t kModeMask   = 0x0018u;
inline constexpr uint32_t kModeShift  = 3u;
inline constexpr uint16_t kScanType   = 1u << 5;
inline constexpr uint16_t kScanStart  = 1u << 6;
inline constexpr uint16_t kScanStop   = 1u << 7;
inline constexpr uint16_t kAtStart    = 1u << 8;
inline constexpr uint16_t kAtStop     = 1u << 9;
inline constexpr uint32_t kStateShift = 10u;
inline constexpr uint16_t kPenStc     = 1u << 13;
inline constexpr uint16_t kPenStp     = 1u << 14;
inline constexpr uint16_t kPadStop    = 1u << 15;
inline constexpr uint16_t kCntStored  = kPiuPwr | kSeqEn | kModeMask | kScanType | kScanStart | kScanStop |
                                       kAtStart | kAtStop | kPadStop;

inline constexpr uint16_t kStblModel   = 1u << 8;
inline constexpr uint16_t kAdCmdMask   = 0x000Fu;
inline constexpr uint32_t kPenCntShift = 4u;
inline constexpr uint16_t kPenCntBits  = 0x001Fu;

enum : uint16_t {
    kStDisable  = 0u,
    kStStandby  = 1u,
    kStMainBatt = 2u,
    kStSubBatt  = 3u,
    kStWaitPen  = 4u,
    kStPenScan  = 5u,
    kStInterval = 6u,
    kStCommand  = 7u,
};

}
