#include "compactflash_data_transfer.h"

#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

void CompactFlashDataTransfer::Start(CompactFlashTransfer kind) {
    kind_      = kind;
    pos_       = 0;
    odd_first_ = false;
}

bool CompactFlashDataTransfer::In() const {
    return kind_ == CompactFlashTransfer::kReadSectors || kind_ == CompactFlashTransfer::kReadOnce ||
           kind_ == CompactFlashTransfer::kReadLong;
}

bool CompactFlashDataTransfer::Out() const {
    return kind_ == CompactFlashTransfer::kWriteSectors || kind_ == CompactFlashTransfer::kWriteVerify ||
           kind_ == CompactFlashTransfer::kWriteBuffer || kind_ == CompactFlashTransfer::kWriteLong ||
           kind_ == CompactFlashTransfer::kFormatTrack;
}

uint32_t CompactFlashDataTransfer::Bytes() const {
    const bool long_sector =
        kind_ == CompactFlashTransfer::kReadLong || kind_ == CompactFlashTransfer::kWriteLong;
    return long_sector ? kSectorBytes + kEccBytes : kSectorBytes;
}

uint8_t& CompactFlashDataTransfer::ByteAt(uint32_t pos) {
    return pos < kSectorBytes ? buf_[pos] : ecc_[pos - kSectorBytes];
}

void CompactFlashDataTransfer::RequireNoRepeatedOddByte(const char* access) const {
    if (!odd_first_) return;
    emu_.Get<Fatal>().Die("[CF] repeated odd data register byte %s at buffer position %u", access, pos_);
}

uint8_t CompactFlashDataTransfer::ReadByte() {
    const uint8_t v = ByteAt(pos_);
    pos_ += odd_first_ ? 2u : 1u;
    odd_first_ = false;
    return v;
}

uint8_t CompactFlashDataTransfer::ReadOddByte() {
    if ((pos_ & 1u) != 0u)
        return ReadByte();
    RequireNoRepeatedOddByte("read");
    odd_first_ = true;
    return ByteAt(pos_ + 1u);
}

uint16_t CompactFlashDataTransfer::ReadWord() {
    const uint16_t v = cerf::le::U16(buf_.data(), pos_);
    pos_ += 2u;
    return v;
}

void CompactFlashDataTransfer::WriteByte(uint8_t value) {
    ByteAt(pos_) = value;
    pos_ += odd_first_ ? 2u : 1u;
    odd_first_ = false;
}

void CompactFlashDataTransfer::WriteOddByte(uint8_t value) {
    if ((pos_ & 1u) != 0u) {
        WriteByte(value);
        return;
    }
    RequireNoRepeatedOddByte("write");
    odd_first_ = true;
    ByteAt(pos_ + 1u) = value;
}

void CompactFlashDataTransfer::WriteWord(uint16_t value) {
    cerf::le::Put16(buf_.data() + pos_, value);
    pos_ += 2u;
}

void CompactFlashDataTransfer::SaveState(StateWriter& w) const {
    w.Write("transfer", static_cast<uint8_t>(kind_));
    w.WriteBytes("buf", buf_.data(), buf_.size());
    w.WriteBytes("ecc", ecc_.data(), ecc_.size());
    w.Write("buf_pos", pos_);
    w.Write<uint8_t>("odd_first", odd_first_ ? 1u : 0u);
}

void CompactFlashDataTransfer::RestoreState(StateReader& r) {
    uint8_t kind = 0;
    r.Read("transfer", kind);
    r.ReadBytes("buf", buf_.data(), buf_.size());
    r.ReadBytes("ecc", ecc_.data(), ecc_.size());
    r.Read("buf_pos", pos_);
    uint8_t odd_first = 0;
    r.Read("odd_first", odd_first);
    kind_      = static_cast<CompactFlashTransfer>(kind);
    odd_first_ = odd_first != 0u;
}

}
