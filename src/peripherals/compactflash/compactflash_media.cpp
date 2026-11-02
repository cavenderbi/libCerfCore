#include "compactflash_media.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>

#include <array>
#include <cstring>

namespace cerfcore {

namespace {

constexpr uint32_t kSectorBytes  = CompactFlashImage::kSectorBytes;
constexpr uint8_t  kFormatPattern = 0xFFu;

constexpr std::size_t kTranslateCylinderMsb = 0x00u;
constexpr std::size_t kTranslateCylinderLsb = 0x01u;
constexpr std::size_t kTranslateHead        = 0x02u;
constexpr std::size_t kTranslateSector      = 0x03u;
constexpr std::size_t kTranslateLbaMsb      = 0x04u;
constexpr std::size_t kTranslateLbaMid      = 0x05u;
constexpr std::size_t kTranslateLbaLsb      = 0x06u;
constexpr std::size_t kTranslateErased      = 0x13u;
constexpr uint8_t     kTranslateNotErased   = 0x00u;

}

CompactFlashMedia::CompactFlashMedia(CerfEmulator& emu, const std::string& image_path)
    : emu_(emu), image_(image_path) {}

void CompactFlashMedia::ReadSector(uint64_t lba, uint8_t* out) {
    if (!image_.ReadSector(lba, out))
        emu_.Get<Fatal>().UserError("[CF] reading sector %llu of the CompactFlash image failed on the host",
                                    static_cast<unsigned long long>(lba));
}

void CompactFlashMedia::WriteSector(uint64_t lba, const uint8_t* in) {
    if (!image_.WriteSector(lba, in))
        emu_.Get<Fatal>().UserError("[CF] writing sector %llu of the CompactFlash image failed on the host",
                                    static_cast<unsigned long long>(lba));
}

CompactFlashAddressFault CompactFlashMedia::Check(const CompactFlashTaskFile& tf) const {
    uint64_t lba = 0;
    return tf.Resolve(TotalSectors(), &lba);
}

CompactFlashAddressFault CompactFlashMedia::Seek(const CompactFlashTaskFile& tf) const {
    uint64_t lba = 0;
    return tf.ResolveCylinderHead(TotalSectors(), &lba);
}

CompactFlashAddressFault CompactFlashMedia::Read(const CompactFlashTaskFile& tf, uint8_t* out) {
    uint64_t lba = 0;
    const CompactFlashAddressFault fault = tf.Resolve(TotalSectors(), &lba);
    if (fault == CompactFlashAddressFault::kNone)
        ReadSector(lba, out);
    return fault;
}

CompactFlashAddressFault CompactFlashMedia::Write(const CompactFlashTaskFile& tf, const uint8_t* in,
                                                  bool verify) {
    uint64_t lba = 0;
    const CompactFlashAddressFault fault = tf.Resolve(TotalSectors(), &lba);
    if (fault != CompactFlashAddressFault::kNone)
        return fault;
    WriteSector(lba, in);
    if (!verify)
        return fault;
    std::array<uint8_t, kSectorBytes> written{};
    ReadSector(lba, written.data());
    if (std::memcmp(written.data(), in, kSectorBytes) != 0)
        emu_.Get<Fatal>().UserError("[CF] sector %llu of the CompactFlash image reads back different data",
                                    static_cast<unsigned long long>(lba));
    return fault;
}

CompactFlashAddressFault CompactFlashMedia::Format(const CompactFlashTaskFile& tf) {
    uint64_t first = 0;
    const CompactFlashAddressFault fault = tf.ResolveCylinderHead(TotalSectors(), &first);
    if (fault != CompactFlashAddressFault::kNone)
        return fault;
    const bool lba_mode = (tf.drv_head & CompactFlashTaskFile::kHeadLba) != 0u;
    const uint32_t count = lba_mode ? tf.SectorCount() : tf.geometry.sectors;
    if (first + count > TotalSectors())
        return CompactFlashAddressFault::kAddressTooLarge;
    std::array<uint8_t, kSectorBytes> pattern{};
    pattern.fill(kFormatPattern);
    for (uint32_t i = 0; i < count; ++i)
        WriteSector(first + i, pattern.data());
    return fault;
}

CompactFlashAddressFault CompactFlashMedia::Translate(const CompactFlashTaskFile& tf, uint8_t* out) {
    uint64_t lba = 0;
    const CompactFlashAddressFault fault = tf.Resolve(TotalSectors(), &lba);
    if (fault != CompactFlashAddressFault::kNone)
        return fault;
    const uint64_t track    = lba / tf.geometry.sectors;
    const uint64_t cylinder = track / tf.geometry.heads;
    std::memset(out, 0, kSectorBytes);
    out[kTranslateCylinderMsb] = static_cast<uint8_t>(cylinder >> 8);
    out[kTranslateCylinderLsb] = static_cast<uint8_t>(cylinder);
    out[kTranslateHead]        = static_cast<uint8_t>(track % tf.geometry.heads);
    out[kTranslateSector]      = static_cast<uint8_t>(lba % tf.geometry.sectors + 1u);
    out[kTranslateLbaMsb]      = static_cast<uint8_t>(lba >> 16);
    out[kTranslateLbaMid]      = static_cast<uint8_t>(lba >> 8);
    out[kTranslateLbaLsb]      = static_cast<uint8_t>(lba);
    out[kTranslateErased]      = kTranslateNotErased;
    return fault;
}

}
