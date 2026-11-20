#include "intel_i2c_unit.h"

#include "i2c_slave.h"
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kIcrMask   = 0x0000FFFFu;
constexpr uint32_t kIcrUr     = 1u << 14;
constexpr uint32_t kIcrAldie  = 1u << 12;
constexpr uint32_t kIcrSsdie  = 1u << 11;
constexpr uint32_t kIcrBeie   = 1u << 10;
constexpr uint32_t kIcrDrfie  = 1u << 9;
constexpr uint32_t kIcrIteie  = 1u << 8;
constexpr uint32_t kIcrIue    = 1u << 6;
constexpr uint32_t kIcrScle   = 1u << 5;
constexpr uint32_t kIcrMa     = 1u << 4;
constexpr uint32_t kIcrTb     = IntelI2cUnit::kIcrTransferByte;
constexpr uint32_t kIcrAcknak = 1u << 2;
constexpr uint32_t kIcrStop   = 1u << 1;
constexpr uint32_t kIcrStart  = 1u << 0;

constexpr uint32_t kIsrBed    = 1u << 10;
constexpr uint32_t kIsrIrf    = 1u << 7;
constexpr uint32_t kIsrIte    = 1u << 6;
constexpr uint32_t kIsrAld    = 1u << 5;
constexpr uint32_t kIsrSsd    = 1u << 4;
constexpr uint32_t kIsrIbb    = 1u << 3;
constexpr uint32_t kIsrUb     = 1u << 2;
constexpr uint32_t kIsrAcknak = 1u << 1;
constexpr uint32_t kIsrRwm    = 1u << 0;
constexpr uint32_t kIsrWriteClear = 0x000007F0u;

}

uint32_t IntelI2cUnit::Isr() const {
    return isr_ | (reading_ ? kIsrRwm : 0u);
}

void IntelI2cUnit::WriteIsr(uint32_t value) {
    isr_ &= ~(value & kIsrWriteClear);
}

void IntelI2cUnit::Reset(I2cSlave* slave) {
    StopTransaction(slave);
    icr_     = 0u;
    isr_     = 0u;
    idbr_    = 0u;
    address_ = 0u;
}

void IntelI2cUnit::WriteIcr(uint32_t value, I2cSlave* slave) {
    if (value & kIcrUr) {
        Reset(slave);
        return;
    }
    const uint32_t control = value & kIcrMask;
    icr_ = control & ~kIcrTb;
    if ((control & kIcrMa) != 0u && (control & kIcrTb) == 0u) {
        StopTransaction(slave);
        isr_ |= kIsrIte;
        return;
    }
    if (control & kIcrTb) {
        isr_ &= ~(kIsrIrf | kIsrIte | kIsrAcknak | kIsrBed);
        TransferByte(control, slave);
    }
    if (control & kIcrStop) StopTransaction(slave);
}

void IntelI2cUnit::TransferByte(uint32_t control, I2cSlave* slave) {
    if (control & kIcrStart) {
        StartAddress(slave);
        return;
    }
    if (selected_ && reading_) {
        idbr_ = slave->TxnReadByte(address_);
        isr_ |= kIsrIrf;
        if (control & kIcrAcknak) isr_ |= kIsrAcknak;
        return;
    }
    isr_ |= kIsrIte;
    if (!selected_ || !slave->TxnWriteByte(address_, static_cast<uint8_t>(idbr_))) NoAcknowledge(slave);
}

void IntelI2cUnit::StartAddress(I2cSlave* slave) {
    reading_  = (idbr_ & 1u) != 0u;
    address_  = static_cast<uint8_t>((idbr_ >> 1) & 0x7Fu);
    isr_     |= kIsrIte | kIsrUb;
    if (slave != nullptr) slave->BusStart();
    selected_ = slave != nullptr && slave->MatchesAddress(address_);
    if (!selected_) {
        NoAcknowledge(slave);
        return;
    }
    slave->TxnStart(address_);
}

void IntelI2cUnit::NoAcknowledge(I2cSlave* slave) {
    isr_ |= kIsrAcknak | kIsrBed;
    StopTransaction(slave);
}

void IntelI2cUnit::StopTransaction(I2cSlave* slave) {
    if (slave != nullptr && (isr_ & kIsrUb) != 0u) slave->BusStop();
    selected_ = false;
    reading_  = false;
    isr_ &= ~(kIsrIbb | kIsrUb);
}

bool IntelI2cUnit::InterruptActive() const {
    return ((isr_ & kIsrAld) && (icr_ & kIcrAldie)) || ((isr_ & kIsrSsd) && (icr_ & kIcrSsdie)) ||
           ((isr_ & kIsrBed) && (icr_ & kIcrBeie)) || ((isr_ & kIsrIrf) && (icr_ & kIcrDrfie)) ||
           ((isr_ & kIsrIte) && (icr_ & kIcrIteie));
}

void IntelI2cUnit::SequencerTransmitted(uint8_t last_byte, bool stop) {
    idbr_     = last_byte;
    icr_      = kIcrIteie | kIcrIue | kIcrScle | (stop ? kIcrStop : 0u);
    isr_     &= ~kIsrIte;
    selected_ = false;
    reading_  = false;
}

void IntelI2cUnit::SaveState(StateWriter& w) const {
    w.Write("icr", icr_); w.Write("isr", isr_); w.Write("idbr", idbr_); w.Write("isar", isar_);
    w.Write("address", address_); w.Write("selected", selected_); w.Write("reading", reading_);
}

void IntelI2cUnit::RestoreState(StateReader& r) {
    r.Read("icr", icr_); r.Read("isr", isr_); r.Read("idbr", idbr_); r.Read("isar", isar_);
    r.Read("address", address_); r.Read("selected", selected_); r.Read("reading", reading_);
}

}
