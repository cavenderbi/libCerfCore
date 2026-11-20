#include "compactflash_card.h"

#include "compactflash_cis.h"
#include "compactflash_image.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../pcmcia/pcmcia_slot.h"
#include <cerfcore/platform/host_path_name.h>
#include <cerfcore/state/state_stream.h>

#include <cstdio>

namespace cerfcore {

namespace {

constexpr uint64_t kSectorsPerMegabyte    = 2048u;
constexpr int64_t  kNsPerSecond           = 1'000'000'000;
constexpr int64_t  kFlashBytesPerSecond   = 20'000'000;
constexpr int64_t  kPhaseBusyNs           = CompactFlashImage::kSectorBytes * kNsPerSecond / kFlashBytesPerSecond;
constexpr uint8_t  kDriveAddressUnknownBit = 0x80u;
constexpr char     kImageNameFormat[]      = "CompactFlash (%s)";
constexpr uint32_t kCisRomEnd              = 0x200u;
constexpr uint8_t  kCisRomBlank            = 0x00u;

}

CompactFlashCard::CompactFlashCard(CerfEmulator& emu, std::string image_path)
    : PcmciaCard(emu), image_path_(std::move(image_path)),
      image_name_(host_platform::HostPathDisplayName(image_path_)), ata_(emu, image_path_) {}

CompactFlashCard::~CompactFlashCard() = default;

std::string CompactFlashCard::DisplayName() const {
    std::string name(image_name_.size() + sizeof(kImageNameFormat), '\0');
    name.resize(static_cast<size_t>(
        std::snprintf(name.data(), name.size(), kImageNameFormat, image_name_.c_str())));
    return name;
}

CardFacts CompactFlashCard::Facts() const {
    const uint64_t sectors = ata_.MediaOpen() ? ata_.TotalSectors() : 0u;
    return StorageCardFacts{image_path_, sectors * (uint64_t{1} << 20) / kSectorsPerMegabyte};
}

void CompactFlashCard::PowerOn() {
    std::lock_guard<std::mutex> lk(mtx_);
    ata_.PowerOn();
    config_.Reset();
    ata_.SetCardNumber(config_.DriveNumber());
    ireq_driven_   = false;
    pulse_request_ = false;
    last_ready_    = ata_.Ready();
}

void CompactFlashCard::PowerOff() {
    std::lock_guard<std::mutex> lk(mtx_);
    ata_.PowerOff();
    UpdateLinesLocked();
}

uint8_t CompactFlashCard::ReadAttribute8(uint32_t offset) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (CompactFlashConfigRegisters::Decodes(offset))
        return config_.Read(offset, ata_.InterruptRequest(), ata_.Ready());
    if (offset < kCompactFlashCisSize * 2u) return kCompactFlashCis[offset / 2u];
    if (offset < kCisRomEnd) return kCisRomBlank;
    emu_.Get<Fatal>().Die("[CF] attribute read at 0x%X decodes to neither the CIS nor a configuration register",
                          offset);
}

void CompactFlashCard::WriteAttribute8(uint32_t offset, uint8_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (offset < kCisRomEnd) return;
    if (!CompactFlashConfigRegisters::Decodes(offset))
        emu_.Get<Fatal>().Die("[CF] attribute write 0x%02X at 0x%X decodes to no configuration register",
                              value, offset);
    const bool was_reset      = config_.SoftResetAsserted();
    const bool was_power_down = config_.PowerDownRequested();
    const uint8_t reserved = config_.Write(offset, value);
    if (reserved != 0u)
        emu_.Get<Fatal>().Die("[CF] configuration register write 0x%02X at 0x%X sets reserved bits 0x%02X",
                              value, offset, reserved);
    if (config_.SoftResetAsserted() != was_reset)
        ata_.HoldCardReset(config_.SoftResetAsserted());
    if (config_.PowerDownRequested() != was_power_down)
        ata_.SetPowerDown(config_.PowerDownRequested());
    ata_.SetCardNumber(config_.DriveNumber());
    UpdateLinesLocked();
}

uint8_t CompactFlashCard::ReadCommon8(uint32_t offset) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint8_t v = ReadRegisterLocked(CompactFlashSpace::kCommonMemory, offset);
    UpdateLinesLocked();
    return v;
}

uint16_t CompactFlashCard::ReadCommon16(uint32_t offset) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint16_t v = ReadWordLocked(CompactFlashSpace::kCommonMemory, offset);
    UpdateLinesLocked();
    return v;
}

void CompactFlashCard::WriteCommon8(uint32_t offset, uint8_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    WriteRegisterLocked(CompactFlashSpace::kCommonMemory, offset, value);
    UpdateLinesLocked();
}

void CompactFlashCard::WriteCommon16(uint32_t offset, uint16_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    WriteWordLocked(CompactFlashSpace::kCommonMemory, offset, value);
    UpdateLinesLocked();
}

uint8_t CompactFlashCard::ReadIo8(uint32_t offset) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint8_t v = ReadRegisterLocked(CompactFlashSpace::kIo, offset);
    UpdateLinesLocked();
    return v;
}

uint16_t CompactFlashCard::ReadIo16(uint32_t offset) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint16_t v = ReadWordLocked(CompactFlashSpace::kIo, offset);
    UpdateLinesLocked();
    return v;
}

void CompactFlashCard::WriteIo8(uint32_t offset, uint8_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    WriteRegisterLocked(CompactFlashSpace::kIo, offset, value);
    UpdateLinesLocked();
}

void CompactFlashCard::WriteIo16(uint32_t offset, uint16_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    WriteWordLocked(CompactFlashSpace::kIo, offset, value);
    UpdateLinesLocked();
}

bool CompactFlashCard::Iois16(uint32_t offset) {
    std::lock_guard<std::mutex> lk(mtx_);
    return RegisterAtLocked(CompactFlashSpace::kIo, offset) != CompactFlashReg::kNone;
}

bool CompactFlashCard::InputAcknowledge(uint32_t offset) {
    std::lock_guard<std::mutex> lk(mtx_);
    return config_.IoInterface() && RegisterAtLocked(CompactFlashSpace::kIo, offset) != CompactFlashReg::kNone;
}

int CompactFlashCard::RegisterAtLocked(CompactFlashSpace space, uint32_t offset) const {
    return CompactFlashRegisterAt(config_.ConfigIndex(), space, offset);
}

uint16_t CompactFlashCard::ReadWordLocked(CompactFlashSpace space, uint32_t offset) {
    const uint32_t even = offset & ~1u;
    if (CompactFlashWordDataRegister(RegisterAtLocked(space, even))) {
        ata_.HostAccess();
        return ata_.ReadData16();
    }
    const uint16_t low = ReadRegisterLocked(space, even);
    const uint16_t high = ReadRegisterLocked(space, even | 1u);
    return static_cast<uint16_t>(low | (high << 8));
}

void CompactFlashCard::WriteWordLocked(CompactFlashSpace space, uint32_t offset, uint16_t value) {
    const uint32_t even = offset & ~1u;
    if (CompactFlashWordDataRegister(RegisterAtLocked(space, even))) {
        ata_.HostAccess();
        ata_.WriteData16(value);
        return;
    }
    WriteRegisterLocked(space, even, static_cast<uint8_t>(value & 0xFFu));
    WriteRegisterLocked(space, even | 1u, static_cast<uint8_t>(value >> 8));
}

uint8_t CompactFlashCard::ReadRegisterLocked(CompactFlashSpace space, uint32_t offset) {
    ata_.HostAccess();
    const CompactFlashTaskFile& tf = ata_.TaskFile();
    switch (RegisterAtLocked(space, offset)) {
        case CompactFlashReg::kData:
        case CompactFlashReg::kDupEvenData:
            return ata_.ReadData8();
        case CompactFlashReg::kDupOddData:
            return ata_.ReadOddData8();
        case CompactFlashReg::kError:
        case CompactFlashReg::kDupError:
            return ata_.ReadError();
        case CompactFlashReg::kSectCnt:
            return tf.sect_cnt;
        case CompactFlashReg::kSectNum:
            return tf.sect_num;
        case CompactFlashReg::kCylLow:
            return tf.cyl_low;
        case CompactFlashReg::kCylHigh:
            return tf.cyl_high;
        case CompactFlashReg::kDrvHead:
            return tf.drv_head;
        case CompactFlashReg::kStatus:
            return ata_.ReadStatus();
        case CompactFlashReg::kDevCtl:
            return ata_.AltStatus();
        case CompactFlashReg::kDriveAddress:
            return static_cast<uint8_t>((slot_->UndrivenDataLane() & kDriveAddressUnknownBit) |
                                        ata_.ReadDriveAddress());
        case CompactFlashReg::kNone:
            return slot_->UndrivenDataLane();
        default:
            emu_.Get<Fatal>().Die("[CF] register read at 0x%X decodes to an unmapped register index", offset);
    }
}

void CompactFlashCard::WriteRegisterLocked(CompactFlashSpace space, uint32_t offset, uint8_t value) {
    ata_.HostAccess();
    switch (RegisterAtLocked(space, offset)) {
        case CompactFlashReg::kData:
        case CompactFlashReg::kDupEvenData:
            ata_.WriteData8(value);
            return;
        case CompactFlashReg::kDupOddData:
            ata_.WriteOddData8(value);
            return;
        case CompactFlashReg::kFeature:
        case CompactFlashReg::kDupFeature:
            ata_.WriteTaskFile(&CompactFlashTaskFile::feature, value);
            return;
        case CompactFlashReg::kSectCnt:
            ata_.WriteTaskFile(&CompactFlashTaskFile::sect_cnt, value);
            return;
        case CompactFlashReg::kSectNum:
            ata_.WriteTaskFile(&CompactFlashTaskFile::sect_num, value);
            return;
        case CompactFlashReg::kCylLow:
            ata_.WriteTaskFile(&CompactFlashTaskFile::cyl_low, value);
            return;
        case CompactFlashReg::kCylHigh:
            ata_.WriteTaskFile(&CompactFlashTaskFile::cyl_high, value);
            return;
        case CompactFlashReg::kDrvHead:
            ata_.WriteTaskFile(&CompactFlashTaskFile::drv_head, value);
            return;
        case CompactFlashReg::kDevCtl:
            ata_.WriteDeviceControl(value);
            return;
        case CompactFlashReg::kCommand:
            ata_.WriteCommand(value);
            return;
        case CompactFlashReg::kDriveAddress:
        case CompactFlashReg::kNone:
            return;
        default:
            emu_.Get<Fatal>().Die("[CF] register write 0x%02X at 0x%X decodes to an unmapped register index",
                                  value, offset);
    }
}

bool CompactFlashCard::PinLowLocked() const {
    if (!config_.IoInterface()) return !ata_.Ready();
    return ata_.InterruptRequest() && config_.LevelInterrupts();
}

void CompactFlashCard::UpdateLinesLocked() {
    if (ata_.TakeBusyStart()) slot_->ArmCardTimer(kPhaseBusyNs);
    const bool ready = ata_.Ready();
    if (ready != last_ready_) {
        last_ready_ = ready;
        config_.NoteReadyChange();
    }
    slot_->DriveStatusChange(config_.StatusChangeAsserted());
    const bool request = ata_.InterruptRequest() && config_.IoInterface();
    const bool level   = config_.LevelInterrupts();
    const bool pulse   = !level && request && !pulse_request_;
    pulse_request_ = request && !level;
    const bool drive = PinLowLocked();
    if (drive != ireq_driven_) {
        ireq_driven_ = drive;
        if (drive) slot_->RaiseIrq();
        else       slot_->ClearIrq();
    }
    if (pulse) {
        slot_->RaiseIrq();
        slot_->ClearIrq();
    }
}

void CompactFlashCard::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(mtx_);
    ata_.SaveState(w);
    config_.SaveState(w);
}

void CompactFlashCard::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(mtx_);
    ata_.RestoreState(r);
    config_.RestoreState(r);
    ata_.SetCardNumber(config_.DriveNumber());
}

void CompactFlashCard::PostRestore() {
    std::lock_guard<std::mutex> lk(mtx_);
    const bool request = ata_.InterruptRequest() && config_.IoInterface();
    ireq_driven_   = PinLowLocked();
    pulse_request_ = request && !config_.LevelInterrupts();
    last_ready_    = ata_.Ready();
    if (ireq_driven_) slot_->RaiseIrq();
    else              slot_->ClearIrq();
    slot_->DriveStatusChange(config_.StatusChangeAsserted());
}

void CompactFlashCard::OnSlotTimer() {
    std::lock_guard<std::mutex> lk(mtx_);
    ata_.EndBusy();
    UpdateLinesLocked();
}

}
