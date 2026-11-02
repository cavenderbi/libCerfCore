#include "compactflash_ata.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

using Tf = CompactFlashTaskFile;

constexpr uint8_t  kIdleDataRead8           = 0xFFu;
constexpr uint16_t kIdleDataRead16          = 0xFFFFu;
constexpr uint8_t  kAbsentDeviceStatus      = 0x00u;
constexpr uint8_t  kAbsentDeviceError       = 0x00u;
constexpr uint8_t  kDriveAddressNoWriteGate = 0x40u;
constexpr uint8_t  kDriveAddressHeadMask    = 0x0Fu;
constexpr uint8_t  kDriveAddressHeadShift   = 2u;
constexpr uint8_t  kDriveAddressNoDrive     = 0x03u;

}

CompactFlashAta::CompactFlashAta(CerfEmulator& emu, const std::string& image_path)
    : core_(emu, image_path), commands_(core_) {}

void CompactFlashAta::PowerOn() {
    core_.PowerOn();
    in_reset_      = false;
    in_card_reset_ = false;
}

void CompactFlashAta::PowerOff() {
    core_.PowerOff();
    in_reset_      = false;
    in_card_reset_ = false;
}

bool CompactFlashAta::Ready() const {
    return (core_.tf.status & Tf::kStatusBusy) == 0u;
}

void CompactFlashAta::EnterReset() {
    core_.IrqClear();
    core_.xfer.Stop();
    core_.sectors_left = 0;
    core_.tf.status    = Tf::kStatusBusy;
}

void CompactFlashAta::HoldCardReset(bool asserted) {
    if (asserted) {
        EnterReset();
        in_card_reset_ = true;
        return;
    }
    if (!in_card_reset_) return;
    in_card_reset_ = false;
    in_reset_      = false;
    core_.PowerOn();
}

void CompactFlashAta::SetPowerDown(bool requested) {
    if (requested) core_.power.Sleep();
    else           core_.power.Wake();
}

uint8_t CompactFlashAta::SelectedStatus() const {
    return Selected() ? core_.tf.status : kAbsentDeviceStatus;
}

uint8_t CompactFlashAta::ReadStatus() {
    core_.IrqClear();
    return SelectedStatus();
}

uint8_t CompactFlashAta::ReadError() const {
    return Selected() ? core_.tf.error : kAbsentDeviceError;
}

uint8_t CompactFlashAta::ReadDriveAddress() const {
    uint8_t v = static_cast<uint8_t>((~core_.tf.drv_head & kDriveAddressHeadMask) << kDriveAddressHeadShift);
    if (!core_.xfer.Out()) v |= kDriveAddressNoWriteGate;
    v |= kDriveAddressNoDrive;
    if (Selected()) v = static_cast<uint8_t>(v & ~(1u << card_number_));
    return v;
}

void CompactFlashAta::WriteTaskFile(uint8_t CompactFlashTaskFile::* field, uint8_t value) {
    if (Resetting()) return;
    core_.tf.*field = value;
}

void CompactFlashAta::WriteDeviceControl(uint8_t value) {
    const bool reset = (value & Tf::kDevCtlSoftReset) != 0u;
    core_.tf.dev_ctrl = value;
    if (in_card_reset_ || reset == in_reset_) return;
    in_reset_ = reset;
    if (reset) {
        EnterReset();
        return;
    }
    core_.SoftResetComplete();
}

void CompactFlashAta::WriteCommand(uint8_t command) {
    core_.IrqClear();
    if (Resetting() || !Selected())
        return;
    core_.tf.error = 0;
    const bool was_sleeping = core_.power.Sleeping();
    core_.power.Wake();
    core_.xfer.Stop();
    commands_.Execute(command, was_sleeping);
}

bool CompactFlashAta::DataPortIn() const {
    return Selected() && core_.xfer.In();
}

bool CompactFlashAta::DataPortOut() const {
    return Selected() && core_.xfer.Out();
}

void CompactFlashAta::AfterRead() {
    if (core_.xfer.BufferDone())
        commands_.OnReadBufferEnd();
}

void CompactFlashAta::AfterWrite() {
    if (core_.xfer.BufferDone())
        commands_.OnWriteBufferFull();
}

uint8_t CompactFlashAta::ReadData8() {
    if (!DataPortIn())
        return kIdleDataRead8;
    const uint8_t v = core_.xfer.ReadByte();
    AfterRead();
    return v;
}

uint8_t CompactFlashAta::ReadOddData8() {
    if (!DataPortIn())
        return kIdleDataRead8;
    const uint8_t v = core_.xfer.ReadOddByte();
    AfterRead();
    return v;
}

uint16_t CompactFlashAta::ReadData16() {
    if (!DataPortIn())
        return kIdleDataRead16;
    if (!core_.xfer.WordAligned()) {
        const uint16_t low  = ReadData8();
        const uint16_t high = ReadData8();
        return static_cast<uint16_t>(low | (high << 8));
    }
    const uint16_t v = core_.xfer.ReadWord();
    AfterRead();
    return v;
}

void CompactFlashAta::WriteData8(uint8_t value) {
    if (!DataPortOut())
        return;
    core_.xfer.WriteByte(value);
    AfterWrite();
}

void CompactFlashAta::WriteOddData8(uint8_t value) {
    if (!DataPortOut())
        return;
    core_.xfer.WriteOddByte(value);
    AfterWrite();
}

void CompactFlashAta::WriteData16(uint16_t value) {
    if (!DataPortOut())
        return;
    if (!core_.xfer.WordAligned()) {
        WriteData8(static_cast<uint8_t>(value & 0xFFu));
        WriteData8(static_cast<uint8_t>(value >> 8));
        return;
    }
    core_.xfer.WriteWord(value);
    AfterWrite();
}

void CompactFlashAta::SaveState(StateWriter& w) {
    core_.SaveState(w);
    w.Write<uint8_t>("soft_reset_held", in_reset_ ? 1u : 0u);
    w.Write<uint8_t>("card_reset_held", in_card_reset_ ? 1u : 0u);
}

void CompactFlashAta::RestoreState(StateReader& r) {
    core_.RestoreState(r);
    uint8_t held = 0;
    r.Read("soft_reset_held", held);
    in_reset_ = held != 0u;
    uint8_t card_held = 0;
    r.Read("card_reset_held", card_held);
    in_card_reset_ = card_held != 0u;
}

}
