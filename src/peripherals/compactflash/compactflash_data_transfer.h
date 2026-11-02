#pragma once

#include "compactflash_image.h"

#include <array>
#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

enum class CompactFlashTransfer : uint8_t {
    kNone,
    kReadSectors,
    kReadOnce,
    kReadLong,
    kWriteSectors,
    kWriteVerify,
    kWriteBuffer,
    kWriteLong,
    kFormatTrack,
};

class CompactFlashDataTransfer {
public:
    static constexpr uint32_t kSectorBytes = CompactFlashImage::kSectorBytes;

    explicit CompactFlashDataTransfer(CerfEmulator& emu) : emu_(emu) {}

    void Start(CompactFlashTransfer kind);
    void Stop() { kind_ = CompactFlashTransfer::kNone; }
    CompactFlashTransfer Kind() const { return kind_; }
    bool In() const;
    bool Out() const;
    bool BufferDone() const { return pos_ >= Bytes(); }
    bool WordAligned() const { return (pos_ & 1u) == 0u && pos_ + 2u <= kSectorBytes && !odd_first_; }

    std::array<uint8_t, kSectorBytes>& Buffer() { return buf_; }
    void FillEcc(uint8_t value) { ecc_.fill(value); }

    uint8_t  ReadByte();
    uint8_t  ReadOddByte();
    uint16_t ReadWord();
    void     WriteByte(uint8_t value);
    void     WriteOddByte(uint8_t value);
    void     WriteWord(uint16_t value);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    static constexpr uint32_t kEccBytes = 4u;

    uint32_t Bytes() const;
    uint8_t& ByteAt(uint32_t pos);
    void     RequireNoRepeatedOddByte(const char* access) const;

    CerfEmulator&                     emu_;
    std::array<uint8_t, kSectorBytes> buf_{};
    std::array<uint8_t, kEccBytes>    ecc_{};
    CompactFlashTransfer              kind_      = CompactFlashTransfer::kNone;
    uint32_t                          pos_       = 0;
    bool                              odd_first_ = false;
};

}
