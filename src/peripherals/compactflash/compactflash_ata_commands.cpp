#include "compactflash_ata_commands.h"

namespace cerfcore {

namespace CompactFlashCommand {

namespace {

constexpr uint8_t kFeatureEnable8Bit              = 0x01u;
constexpr uint8_t kFeatureTransferMode            = 0x03u;
constexpr uint8_t kFeatureVendorEcc               = 0x44u;
constexpr uint8_t kFeatureDisableLookAhead        = 0x55u;
constexpr uint8_t kFeatureKeepSettingsOnSoftReset = 0x66u;
constexpr uint8_t kFeatureCompatibilityNop69      = 0x69u;
constexpr uint8_t kFeatureDisable8Bit             = 0x81u;
constexpr uint8_t kFeatureCompatibilityNop96      = 0x96u;
constexpr uint8_t kFeatureCompatibility97         = 0x97u;
constexpr uint8_t kFeatureFourEccBytes            = 0xBBu;
constexpr uint8_t kFeatureDefaultsOnSoftReset     = 0xCCu;

constexpr uint8_t kTransferTypeShift          = 3u;
constexpr uint8_t kTransferModeMask           = 0x07u;
constexpr uint8_t kTransferTypePioDefault     = 0x00u;
constexpr uint8_t kTransferTypePioFlowControl = 0x01u;
constexpr uint8_t kPioDefaultMode             = 0x00u;
constexpr uint8_t kHighestPioMode             = 0x02u;

bool TransferModeSupported(uint8_t sector_count) {
    const uint8_t type = static_cast<uint8_t>(sector_count >> kTransferTypeShift);
    const uint8_t mode = static_cast<uint8_t>(sector_count & kTransferModeMask);
    if (type == kTransferTypePioDefault)
        return mode == kPioDefaultMode;
    if (type == kTransferTypePioFlowControl)
        return mode <= kHighestPioMode;
    return false;
}

}

CompactFlashFeatureResult ClassifyFeature(uint8_t feature, uint8_t sector_count) {
    switch (feature) {
        case kFeatureEnable8Bit:
        case kFeatureDisable8Bit:
        case kFeatureVendorEcc:
        case kFeatureFourEccBytes:
        case kFeatureDisableLookAhead:
        case kFeatureCompatibilityNop69:
        case kFeatureCompatibilityNop96:
        case kFeatureCompatibility97:
            return CompactFlashFeatureResult::kAccept;
        case kFeatureTransferMode:
            return TransferModeSupported(sector_count) ? CompactFlashFeatureResult::kAccept
                                                       : CompactFlashFeatureResult::kAbort;
        case kFeatureKeepSettingsOnSoftReset:
            return CompactFlashFeatureResult::kKeepSettingsOnSoftReset;
        case kFeatureDefaultsOnSoftReset:
            return CompactFlashFeatureResult::kDefaultsOnSoftReset;
        default:
            return CompactFlashFeatureResult::kAbort;
    }
}

}

}
