#include "compactflash_config_registers.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kCorOffset  = 0x200u;
constexpr uint32_t kCcsrOffset = 0x202u;
constexpr uint32_t kPrrOffset  = 0x204u;
constexpr uint32_t kScrOffset  = 0x206u;

constexpr uint8_t kCorSoftReset      = 0x80u;
constexpr uint8_t kCorLevelIreq      = 0x40u;
constexpr uint8_t kCorReservedIndex  = 0x30u;
constexpr uint8_t kCorConfigIndex    = 0x3Fu;
constexpr uint8_t kCorLastIndex      = 0x03u;

constexpr uint8_t kCcsrChanged       = 0x80u;
constexpr uint8_t kCcsrSigChg        = 0x40u;
constexpr uint8_t kCcsrIois8         = 0x20u;
constexpr uint8_t kCcsrAudio         = 0x08u;
constexpr uint8_t kCcsrPowerDown     = 0x04u;
constexpr uint8_t kCcsrIntr          = 0x02u;
constexpr uint8_t kCcsrStored        = kCcsrSigChg | kCcsrIois8 | kCcsrAudio | kCcsrPowerDown;

constexpr uint8_t kPrrCReady         = 0x20u;
constexpr uint8_t kPrrCWProt         = 0x10u;
constexpr uint8_t kPrrRBvd           = 0x0Cu;
constexpr uint8_t kPrrRReady         = 0x02u;
constexpr uint8_t kPrrMReady         = 0x02u;
constexpr uint8_t kPrrMWProt         = 0x01u;

constexpr uint8_t kScrReserved       = 0x80u;
constexpr uint8_t kScrDriveNumber    = 0x10u;

uint8_t MaskedChange(uint8_t changed, uint8_t value, uint8_t mask_bit, uint8_t changed_bit) {
    if ((value & mask_bit) == 0u) return changed;
    return (value & changed_bit) != 0u ? static_cast<uint8_t>(changed | changed_bit)
                                       : static_cast<uint8_t>(changed & ~changed_bit);
}

}

bool CompactFlashConfigRegisters::Decodes(uint32_t offset) {
    return offset == kCorOffset || offset == kCcsrOffset || offset == kPrrOffset ||
           offset == kScrOffset;
}

void CompactFlashConfigRegisters::Reset() {
    cor_       = 0u;
    ccsr_      = 0u;
    changed_   = 0u;
    scr_drive_ = 0u;
}

uint8_t CompactFlashConfigRegisters::ConfigIndex() const {
    return static_cast<uint8_t>(cor_ & kCorConfigIndex);
}

bool CompactFlashConfigRegisters::IoInterface() const {
    return ConfigIndex() != 0u;
}

bool CompactFlashConfigRegisters::LevelInterrupts() const {
    return (cor_ & kCorLevelIreq) != 0u;
}

bool CompactFlashConfigRegisters::StatusChangeAsserted() const {
    return IoInterface() && (ccsr_ & kCcsrSigChg) != 0u && changed_ != 0u;
}

bool CompactFlashConfigRegisters::SoftResetAsserted() const {
    return (cor_ & kCorSoftReset) != 0u;
}

bool CompactFlashConfigRegisters::PowerDownRequested() const {
    return (ccsr_ & kCcsrPowerDown) != 0u;
}

uint8_t CompactFlashConfigRegisters::DriveNumber() const {
    return scr_drive_ != 0u ? uint8_t{1} : uint8_t{0};
}

void CompactFlashConfigRegisters::NoteReadyChange() {
    changed_ = static_cast<uint8_t>(changed_ | kPrrCReady);
}

uint8_t CompactFlashConfigRegisters::Read(uint32_t offset, bool interrupt_pending, bool ready) const {
    switch (offset) {
        case kCorOffset:
            return cor_;
        case kCcsrOffset:
            return static_cast<uint8_t>((changed_ != 0u ? kCcsrChanged : 0u) | ccsr_ |
                                        (interrupt_pending ? kCcsrIntr : 0u));
        case kPrrOffset:
            return static_cast<uint8_t>(changed_ | kPrrRBvd | (ready ? kPrrRReady : 0u));
        case kScrOffset:
            break;
    }
    return scr_drive_;
}

uint8_t CompactFlashConfigRegisters::Write(uint32_t offset, uint8_t value) {
    switch (offset) {
        case kCorOffset: {
            uint8_t unmodeled = static_cast<uint8_t>(value & kCorReservedIndex);
            if ((value & kCorConfigIndex) > kCorLastIndex)
                unmodeled = static_cast<uint8_t>(unmodeled | (value & kCorConfigIndex));
            if (unmodeled != 0u) return unmodeled;
            if ((value & kCorSoftReset) != 0u) {
                Reset();
                cor_ = kCorSoftReset;
                return 0u;
            }
            cor_ = value;
            return 0u;
        }
        case kCcsrOffset:
            ccsr_ = static_cast<uint8_t>(value & kCcsrStored);
            return 0u;
        case kPrrOffset:
            changed_ = MaskedChange(changed_, value, kPrrMReady, kPrrCReady);
            changed_ = MaskedChange(changed_, value, kPrrMWProt, kPrrCWProt);
            return 0u;
        case kScrOffset:
            break;
    }
    const uint8_t unmodeled = static_cast<uint8_t>(value & kScrReserved);
    if (unmodeled == 0u) scr_drive_ = static_cast<uint8_t>(value & kScrDriveNumber);
    return unmodeled;
}

void CompactFlashConfigRegisters::SaveState(StateWriter& w) const {
    w.Write("cor", cor_);
    w.Write("ccsr", ccsr_);
    w.Write("prr_changed", changed_);
    w.Write("scr_drive", scr_drive_);
}

void CompactFlashConfigRegisters::RestoreState(StateReader& r) {
    r.Read("cor", cor_);
    r.Read("ccsr", ccsr_);
    r.Read("prr_changed", changed_);
    r.Read("scr_drive", scr_drive_);
}

}
