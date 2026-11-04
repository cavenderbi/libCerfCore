#include "i2c_bit_bang_target.h"

#include <cerfcore/state/state_stream.h>
#include "../../socs/i2c_slave.h"

#include <utility>

namespace cerfcore {

I2cBitBangTarget::I2cBitBangTarget(SdaDrive drive, std::vector<I2cSlave*> slaves,
                                   Unmatched unmatched)
    : drive_(std::move(drive)), slaves_(std::move(slaves)), unmatched_(std::move(unmatched)) {}

void I2cBitBangTarget::OnLines(bool scl, bool sda) {
    const bool prev_scl = scl_;
    const bool prev_sda = sda_;
    scl_ = scl;
    sda_ = sda;
    if (scl && prev_scl && sda != prev_sda) {
        if (sda) Stop();
        else     Start();
        return;
    }
    if (scl && !prev_scl)      ClockRise();
    else if (!scl && prev_scl) ClockFall();
}

void I2cBitBangTarget::Start() {
    phase_       = Phase::kAddress;
    shift_       = 0;
    bit_         = 0;
    in_ack_      = false;
    master_nack_ = false;
    slave_       = nullptr;
    drive_(true);
    for (I2cSlave* s : slaves_) s->BusStart();
}

void I2cBitBangTarget::Stop() {
    phase_  = Phase::kIdle;
    bit_    = 0;
    in_ack_ = false;
    drive_(true);
    for (I2cSlave* s : slaves_) s->BusStop();
}

void I2cBitBangTarget::ClockRise() {
    if (phase_ == Phase::kIdle) return;
    if (in_ack_) {
        if (phase_ == Phase::kRead) master_nack_ = sda_;
        return;
    }
    if (bit_ >= 8u) return;
    if (phase_ != Phase::kRead) shift_ = static_cast<uint8_t>((shift_ << 1) | (sda_ ? 1u : 0u));
    ++bit_;
}

void I2cBitBangTarget::ClockFall() {
    if (phase_ == Phase::kIdle) return;
    if (in_ack_) {
        in_ack_ = false;
        bit_    = 0;
        AfterAck();
        return;
    }
    if (bit_ == 8u) {
        in_ack_ = true;
        if (phase_ == Phase::kRead) drive_(true);
        else                        drive_(!AcknowledgeByte());
        return;
    }
    if (phase_ == Phase::kRead) drive_(((shift_ >> (7u - bit_)) & 1u) != 0u);
}

bool I2cBitBangTarget::AcknowledgeByte() {
    if (phase_ == Phase::kAddress) {
        addr_  = static_cast<uint8_t>(shift_ >> 1);
        read_  = (shift_ & 1u) != 0u;
        slave_ = Find(addr_);
        if (!slave_) {
            if (unmatched_) unmatched_(addr_, read_);
            return false;
        }
        slave_->TxnStart(addr_);
        return true;
    }
    slave_->TxnWriteByte(addr_, shift_);
    return true;
}

void I2cBitBangTarget::AfterAck() {
    switch (phase_) {
        case Phase::kAddress:
            if (!slave_) {
                phase_ = Phase::kIdle;
                drive_(true);
                return;
            }
            if (read_) {
                phase_ = Phase::kRead;
                LoadReadByte();
                return;
            }
            phase_ = Phase::kWrite;
            drive_(true);
            return;
        case Phase::kWrite:
            drive_(true);
            return;
        case Phase::kRead:
            if (master_nack_) {
                phase_ = Phase::kIdle;
                drive_(true);
                return;
            }
            LoadReadByte();
            return;
        case Phase::kIdle:
            return;
    }
}

void I2cBitBangTarget::LoadReadByte() {
    shift_ = slave_->TxnReadByte(addr_);
    drive_((shift_ & 0x80u) != 0u);
}

I2cSlave* I2cBitBangTarget::Find(uint8_t addr) const {
    for (I2cSlave* s : slaves_) {
        if (s->MatchesAddress(addr)) return s;
    }
    return nullptr;
}

void I2cBitBangTarget::SaveState(StateWriter& w) const {
    w.Write("i2c_phase", static_cast<uint8_t>(phase_));
    w.Write("i2c_shift", shift_);
    w.Write("i2c_bit", bit_);
    w.Write("i2c_addr", addr_);
    w.Write("i2c_read", static_cast<uint8_t>(read_));
    w.Write("i2c_in_ack", static_cast<uint8_t>(in_ack_));
    w.Write("i2c_master_nack", static_cast<uint8_t>(master_nack_));
    w.Write("i2c_scl", static_cast<uint8_t>(scl_));
    w.Write("i2c_sda", static_cast<uint8_t>(sda_));
    uint8_t selected = kNoSlave;
    for (size_t i = 0; i < slaves_.size(); ++i) {
        if (slaves_[i] == slave_) selected = static_cast<uint8_t>(i);
    }
    w.Write("i2c_slave_index", selected);
}

void I2cBitBangTarget::RestoreState(StateReader& r) {
    uint8_t v = 0;
    r.Read("i2c_phase", v);
    phase_ = static_cast<Phase>(v);
    r.Read("i2c_shift", shift_);
    r.Read("i2c_bit", bit_);
    r.Read("i2c_addr", addr_);
    r.Read("i2c_read", v);        read_        = v != 0u;
    r.Read("i2c_in_ack", v);      in_ack_      = v != 0u;
    r.Read("i2c_master_nack", v); master_nack_ = v != 0u;
    r.Read("i2c_scl", v);         scl_         = v != 0u;
    r.Read("i2c_sda", v);         sda_         = v != 0u;
    r.Read("i2c_slave_index", v);
    slave_ = v == kNoSlave ? nullptr : slaves_[v];
}

}
