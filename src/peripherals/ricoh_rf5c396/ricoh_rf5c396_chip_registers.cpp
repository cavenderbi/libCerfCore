#include "ricoh_rf5c396_chip_registers.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint8_t kRegIdentification = 0x00u;
constexpr uint8_t kRegReserved17     = 0x17u;
constexpr uint8_t kRegModeControl1   = 0x1Fu;
constexpr uint8_t kRegReserved26     = 0x26u;
constexpr uint8_t kRegReserved27     = 0x27u;
constexpr uint8_t kRegReserved2E     = 0x2Eu;
constexpr uint8_t kRegModeControl2   = 0x2Fu;
constexpr uint8_t kRegChipId         = 0x3Au;

constexpr uint8_t kIdentification         = 0x83u;
constexpr uint8_t kChipIdRf5c396          = 0xB2u;
constexpr uint8_t kReservedRegisterDefault = 0x00u;

constexpr uint8_t kCdcCardDetectResume = 0x10u;
constexpr uint8_t kCdcGpiOnRise        = 0x08u;
constexpr uint8_t kCdcReserved         = 0xC0u;

constexpr uint8_t kGlobalExplicitWriteBack = 0x04u;
constexpr uint8_t kGlobalLevelMode         = 0x02u;
constexpr uint8_t kGlobalReserved          = 0xF0u;
constexpr uint8_t kGlobalResetMask         = 0xF0u;

constexpr uint8_t  kMc1Bit7Disable  = 0x80u;
constexpr uint8_t  kMc1AtaMode      = 0x01u;
constexpr unsigned kMc1AtaLineShift = 2u;
constexpr uint32_t kMc1AtaLineMask  = 0x1Fu;
constexpr unsigned kCardAddrLine21  = 21u;

constexpr uint8_t kMc2IreqActiveHigh = 0x02u;
constexpr uint8_t kMc2InpackEnable   = 0x04u;

constexpr uint8_t kMc3InterfaceDisable = 0x01u;
constexpr uint8_t kMc3DmaMode          = 0x02u;
constexpr uint8_t kMc3Reserved         = 0xFCu;

constexpr uint32_t kIsaIoDecodeMask = 0xFFFFu;
constexpr uint32_t kFloppyStatusA   = 0x3F7u;
constexpr uint32_t kFloppyStatusB   = 0x377u;

}

bool RicohRf5c396ChipRegisters::Owns(uint8_t sub) {
    switch (sub) {
        case kRegIdentification:
        case kRegCardDetectControl:
        case kRegReserved17:
        case kRegGlobalControl:
        case kRegModeControl1:
        case kRegReserved26:
        case kRegReserved27:
        case kRegReserved2E:
        case kRegModeControl2:
        case kRegChipId:
        case kRegModeControl3:
            return true;
        default:
            return false;
    }
}

bool RicohRf5c396ChipRegisters::Read(int bank, uint8_t sub, uint8_t* value) const {
    switch (sub) {
        case kRegIdentification:
            *value = kIdentification;
            return true;
        case kRegCardDetectControl:
            *value = card_detect_control_[bank];
            return true;
        case kRegGlobalControl:
            *value = global_control_;
            return true;
        case kRegModeControl1:
            *value = mode_control1_[bank];
            return true;
        case kRegModeControl2:
            *value = mode_control2_[bank];
            return true;
        case kRegChipId:
            *value = kChipIdRf5c396;
            return true;
        case kRegModeControl3:
            *value = mode_control3_[bank];
            return true;
        case kRegReserved17:
        case kRegReserved26:
        case kRegReserved27:
        case kRegReserved2E:
            *value = kReservedRegisterDefault;
            return true;
        default:
            return false;
    }
}

const char* RicohRf5c396ChipRegisters::Write(int bank, uint8_t sub, uint8_t value) {
    switch (sub) {
        case kRegIdentification:
        case kRegChipId:
            return nullptr;
        case kRegCardDetectControl:
            if ((value & kCdcReserved) != 0u) return "sets a reserved bit";
            card_detect_control_[bank] = static_cast<uint8_t>(value & ~kCdcSoftwareCardDetect);
            return nullptr;
        case kRegGlobalControl:
            if ((value & kGlobalReserved) != 0u) return "sets a reserved bit";
            global_control_ = value;
            return nullptr;
        case kRegModeControl1:
            mode_control1_[bank] = value;
            return nullptr;
        case kRegModeControl2:
            mode_control2_[bank] = value;
            return nullptr;
        case kRegModeControl3:
            if ((value & kMc3Reserved) != 0u) return "sets a reserved bit";
            if ((value & kMc3DmaMode) != 0u) return "enables DMA mode, which needs an ISA DMA master";
            mode_control3_[bank] = value;
            return nullptr;
        case kRegReserved17:
        case kRegReserved26:
        case kRegReserved27:
        case kRegReserved2E:
            return "writes a reserved register";
        default:
            return "is not a RF5C396 chip register";
    }
}

void RicohRf5c396ChipRegisters::Reset() {
    for (int bank = 0; bank < 2; ++bank) {
        card_detect_control_[bank] = 0u;
        mode_control1_[bank] = 0u;
        mode_control2_[bank] = 0u;
        mode_control3_[bank] = 0u;
    }
    global_control_ = static_cast<uint8_t>(global_control_ & kGlobalResetMask);
}

bool RicohRf5c396ChipRegisters::ExplicitStatusClear() const {
    return (global_control_ & kGlobalExplicitWriteBack) != 0u;
}

bool RicohRf5c396ChipRegisters::LevelInterrupts() const {
    return (global_control_ & kGlobalLevelMode) != 0u;
}

bool RicohRf5c396ChipRegisters::CardDetectResume(int bank) const {
    return (card_detect_control_[bank] & kCdcCardDetectResume) != 0u;
}

bool RicohRf5c396ChipRegisters::GpiEnabled(int bank) const {
    return (card_detect_control_[bank] & kCdcGpiEnable) != 0u;
}

bool RicohRf5c396ChipRegisters::GpiInterruptOnRise(int bank) const {
    return (card_detect_control_[bank] & kCdcGpiOnRise) != 0u;
}

bool RicohRf5c396ChipRegisters::ConfigurationResetOnRemoval(int bank) const {
    return (card_detect_control_[bank] & kCdcConfigurationReset) != 0u;
}

bool RicohRf5c396ChipRegisters::IreqActiveHigh(int bank) const {
    return (mode_control2_[bank] & kMc2IreqActiveHigh) != 0u;
}

bool RicohRf5c396ChipRegisters::InterfaceDisabled(int bank) const {
    return (mode_control3_[bank] & kMc3InterfaceDisable) != 0u;
}

bool RicohRf5c396ChipRegisters::InpackEnabled(int bank) const {
    return (mode_control2_[bank] & kMc2InpackEnable) != 0u;
}

bool RicohRf5c396ChipRegisters::DisablesDataBit7(int bank, uint32_t isa_io) const {
    const uint32_t port = isa_io & kIsaIoDecodeMask;
    if (port != kFloppyStatusA && port != kFloppyStatusB) return false;
    return (mode_control1_[bank] & kMc1Bit7Disable) != 0u;
}

uint32_t RicohRf5c396ChipRegisters::CardAddressHighLines(int bank, uint32_t card_addr) const {
    if ((mode_control1_[bank] & kMc1AtaMode) == 0u) return card_addr;
    const uint32_t lines = (static_cast<uint32_t>(mode_control1_[bank]) >> kMc1AtaLineShift) & kMc1AtaLineMask;
    return (card_addr & ~(kMc1AtaLineMask << kCardAddrLine21)) | (lines << kCardAddrLine21);
}

void RicohRf5c396ChipRegisters::SaveBank(StateWriter& w, int bank) const {
    w.Write("card_detect_control", card_detect_control_[bank]);
    w.Write("mode_control_1", mode_control1_[bank]);
    w.Write("mode_control_2", mode_control2_[bank]);
    w.Write("mode_control_3", mode_control3_[bank]);
}

void RicohRf5c396ChipRegisters::RestoreBank(StateReader& r, int bank) {
    r.Read("card_detect_control", card_detect_control_[bank]);
    r.Read("mode_control_1", mode_control1_[bank]);
    r.Read("mode_control_2", mode_control2_[bank]);
    r.Read("mode_control_3", mode_control3_[bank]);
}

void RicohRf5c396ChipRegisters::SaveGlobal(StateWriter& w) const {
    w.Write("global_control", global_control_);
}

void RicohRf5c396ChipRegisters::RestoreGlobal(StateReader& r) {
    r.Read("global_control", global_control_);
}

}
