#include "rcvdl56dpfl_frame_check.h"

#include <cerfcore/core/crc32.h>
#include <cerfcore/core/hdlc_fcs16.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

void Rcvdl56dpflFrameCheck::Start(uint8_t fcs_bytes) {
    bytes_ = fcs_bytes;
    value_ = bytes_ == kFcs32Bytes ? 0u : HdlcFcs16::kInit;
}

void Rcvdl56dpflFrameCheck::Update(uint8_t value) {
    if (bytes_ == kFcs32Bytes)
        value_ = Crc32::Update(value_, value);
    else if (bytes_ == kFcs16Bytes)
        value_ = HdlcFcs16::Update(static_cast<uint16_t>(value_), value);
}

uint8_t Rcvdl56dpflFrameCheck::FcsByte(uint8_t index) const {
    const uint32_t fcs = bytes_ == kFcs32Bytes ? value_ : static_cast<uint16_t>(~value_);
    return static_cast<uint8_t>(fcs >> (8u * index));
}

bool Rcvdl56dpflFrameCheck::Good() const {
    return bytes_ == kFcs32Bytes ? value_ == Crc32::kGood : value_ == HdlcFcs16::kGood;
}

void Rcvdl56dpflFrameCheck::SaveState(StateWriter& w, const char* bytes_key, const char* value_key) const {
    w.Write(bytes_key, bytes_);
    w.Write(value_key, value_);
}

void Rcvdl56dpflFrameCheck::RestoreState(StateReader& r, const char* bytes_key, const char* value_key) {
    r.Read(bytes_key, bytes_);
    r.Read(value_key, value_);
}

}
