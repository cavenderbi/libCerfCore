#pragma once

#include <cstdint>

namespace cerfcore {

enum class CompactFlashFeatureResult : uint8_t {
    kAccept,
    kAbort,
    kKeepSettingsOnSoftReset,
    kDefaultsOnSoftReset,
};

namespace CompactFlashCommand {

inline constexpr uint8_t kFamilyMask           = 0xF0u;
inline constexpr uint8_t kNop                  = 0x00u;
inline constexpr uint8_t kRequestSense         = 0x03u;
inline constexpr uint8_t kRecalibrate          = 0x10u;
inline constexpr uint8_t kReadSectors          = 0x20u;
inline constexpr uint8_t kReadSectorsAlt       = 0x21u;
inline constexpr uint8_t kReadLong             = 0x22u;
inline constexpr uint8_t kReadLongAlt          = 0x23u;
inline constexpr uint8_t kWriteSectors         = 0x30u;
inline constexpr uint8_t kWriteSectorsAlt      = 0x31u;
inline constexpr uint8_t kWriteLong            = 0x32u;
inline constexpr uint8_t kWriteLongAlt         = 0x33u;
inline constexpr uint8_t kWriteSectorsNoErase  = 0x38u;
inline constexpr uint8_t kWriteVerify          = 0x3Cu;
inline constexpr uint8_t kReadVerify           = 0x40u;
inline constexpr uint8_t kReadVerifyAlt        = 0x41u;
inline constexpr uint8_t kFormatTrack          = 0x50u;
inline constexpr uint8_t kSeek                 = 0x70u;
inline constexpr uint8_t kTranslateSector      = 0x87u;
inline constexpr uint8_t kDiagnostic           = 0x90u;
inline constexpr uint8_t kInitializeParameters = 0x91u;
inline constexpr uint8_t kStandbyImmediateAlt  = 0x94u;
inline constexpr uint8_t kIdleImmediateAlt     = 0x95u;
inline constexpr uint8_t kStandbyAlt           = 0x96u;
inline constexpr uint8_t kIdleAlt              = 0x97u;
inline constexpr uint8_t kCheckPowerModeAlt    = 0x98u;
inline constexpr uint8_t kSleepAlt             = 0x99u;
inline constexpr uint8_t kEraseSectors         = 0xC0u;
inline constexpr uint8_t kReadMultiple         = 0xC4u;
inline constexpr uint8_t kWriteMultiple        = 0xC5u;
inline constexpr uint8_t kSetMultiple          = 0xC6u;
inline constexpr uint8_t kWriteMultipleNoErase = 0xCDu;
inline constexpr uint8_t kStandbyImmediate     = 0xE0u;
inline constexpr uint8_t kIdleImmediate        = 0xE1u;
inline constexpr uint8_t kStandby              = 0xE2u;
inline constexpr uint8_t kIdle                 = 0xE3u;
inline constexpr uint8_t kReadBuffer           = 0xE4u;
inline constexpr uint8_t kCheckPowerMode       = 0xE5u;
inline constexpr uint8_t kSleep                = 0xE6u;
inline constexpr uint8_t kFlushCache           = 0xE7u;
inline constexpr uint8_t kWriteBuffer          = 0xE8u;
inline constexpr uint8_t kIdentify             = 0xECu;
inline constexpr uint8_t kSetFeatures          = 0xEFu;
inline constexpr uint8_t kWearLevel            = 0xF5u;

CompactFlashFeatureResult ClassifyFeature(uint8_t feature, uint8_t sector_count);

}

namespace CompactFlashSense {

inline constexpr uint8_t kNone            = 0x00u;
inline constexpr uint8_t kSelfTestOk      = 0x01u;
inline constexpr uint8_t kAborted         = 0x1Fu;
inline constexpr uint8_t kInvalidCommand  = 0x20u;
inline constexpr uint8_t kInvalidAddress  = 0x21u;
inline constexpr uint8_t kAddressTooLarge = 0x2Fu;

}

}
