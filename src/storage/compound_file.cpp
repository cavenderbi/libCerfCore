#include "compound_file.h"

#include <cerfcore/core/byte_order.h>

#include <cstring>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint8_t kSignature[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};

constexpr uint32_t kHeaderBytes        = 512u;
constexpr uint32_t kMajorOff           = 26u;
constexpr uint32_t kByteOrderOff       = 28u;
constexpr uint32_t kSectorShiftOff     = 30u;
constexpr uint32_t kMiniShiftOff       = 32u;
constexpr uint32_t kFatSectorsOff      = 44u;
constexpr uint32_t kFirstDirOff        = 48u;
constexpr uint32_t kMiniCutoffOff      = 56u;
constexpr uint32_t kFirstMiniFatOff    = 60u;
constexpr uint32_t kFirstDifatOff      = 68u;
constexpr uint32_t kDifatSectorsOff    = 72u;
constexpr uint32_t kHeaderDifatOff     = 76u;
constexpr uint32_t kHeaderDifatEntries = 109u;

constexpr uint16_t kByteOrderMark  = 0xFFFEu;
constexpr uint16_t kMajorV3        = 3u;
constexpr uint16_t kMajorV4        = 4u;
constexpr uint16_t kSectorShiftV3  = 9u;
constexpr uint16_t kSectorShiftV4  = 12u;
constexpr uint16_t kMiniShift      = 6u;
constexpr uint32_t kMiniSectorSize = 64u;
constexpr uint32_t kMiniCutoff     = 0x1000u;

constexpr uint32_t kMaxRegSect = 0xFFFFFFFAu;
constexpr uint32_t kEndOfChain = 0xFFFFFFFEu;
constexpr uint32_t kFreeSect   = 0xFFFFFFFFu;
constexpr uint32_t kNoStream   = 0xFFFFFFFFu;

constexpr uint32_t kEntryBytes    = 128u;
constexpr uint32_t kEntryNameLen  = 64u;
constexpr uint32_t kEntryTypeOff  = 66u;
constexpr uint32_t kEntryLeftOff  = 68u;
constexpr uint32_t kEntryRightOff = 72u;
constexpr uint32_t kEntryChildOff = 76u;
constexpr uint32_t kEntryStartOff = 116u;
constexpr uint32_t kEntrySizeOff  = 120u;
constexpr uint32_t kNameBytesMax  = 64u;

constexpr uint8_t kTypeStorage = 1u;
constexpr uint8_t kTypeStream  = 2u;
constexpr uint8_t kTypeRoot    = 5u;

}

bool CompoundFile::Fail(const std::string& reason) {
    error_ = reason;
    return false;
}

bool CompoundFile::SectorOffset(uint32_t sector, size_t& offset) {
    if (sector > kMaxRegSect) return Fail("a chain names reserved sector number " + std::to_string(sector));
    const uint64_t at = (uint64_t(sector) + 1u) * sector_size_;
    if (at + sector_size_ > bytes_.size())
        return Fail("sector " + std::to_string(sector) + " lies past the end of the file");
    offset = static_cast<size_t>(at);
    return true;
}

bool CompoundFile::Parse(std::vector<uint8_t> bytes) {
    bytes_ = std::move(bytes);
    fat_.clear();
    mini_fat_.clear();
    entries_.clear();
    mini_stream_.clear();

    if (bytes_.size() < kHeaderBytes) return Fail("the file is shorter than the 512-byte header");
    if (std::memcmp(bytes_.data(), kSignature, sizeof(kSignature)) != 0)
        return Fail("the header signature is not D0 CF 11 E0 A1 B1 1A E1");
    if (cerf::le::U16(bytes_.data(), kByteOrderOff) != kByteOrderMark)
        return Fail("the byte order field is not FFFE");

    major_ = cerf::le::U16(bytes_.data(), kMajorOff);
    const uint16_t shift = cerf::le::U16(bytes_.data(), kSectorShiftOff);
    if (!((major_ == kMajorV3 && shift == kSectorShiftV3) || (major_ == kMajorV4 && shift == kSectorShiftV4)))
        return Fail("major version " + std::to_string(major_) + " with sector shift " + std::to_string(shift));
    if (cerf::le::U16(bytes_.data(), kMiniShiftOff) != kMiniShift)
        return Fail("the mini sector shift is not 6");
    if (cerf::le::U32(bytes_.data(), kMiniCutoffOff) != kMiniCutoff)
        return Fail("the mini stream cutoff is not 4096");
    sector_size_ = 1u << shift;

    return ReadFat() && ReadDirectory() && ReadMiniStream();
}

bool CompoundFile::ReadFat() {
    const uint32_t fat_sectors = cerf::le::U32(bytes_.data(), kFatSectorsOff);
    std::vector<uint32_t> locations;
    for (uint32_t i = 0; i < kHeaderDifatEntries && locations.size() < fat_sectors; ++i)
        locations.push_back(cerf::le::U32(bytes_.data(), kHeaderDifatOff + 4u * i));

    const uint32_t per_difat    = sector_size_ / 4u - 1u;
    const uint32_t difat_count  = cerf::le::U32(bytes_.data(), kDifatSectorsOff);
    uint32_t       difat_sector = cerf::le::U32(bytes_.data(), kFirstDifatOff);
    for (uint32_t n = 0; n < difat_count; ++n) {
        size_t off = 0;
        if (!SectorOffset(difat_sector, off)) return false;
        for (uint32_t i = 0; i < per_difat && locations.size() < fat_sectors; ++i)
            locations.push_back(cerf::le::U32(bytes_.data(), off + 4u * i));
        difat_sector = cerf::le::U32(bytes_.data(), off + 4u * per_difat);
    }
    if (locations.size() != fat_sectors)
        return Fail("the DIFAT holds " + std::to_string(locations.size()) + " of " +
                    std::to_string(fat_sectors) + " FAT sector locations");

    const uint32_t per_sector = sector_size_ / 4u;
    for (uint32_t location : locations) {
        size_t off = 0;
        if (!SectorOffset(location, off)) return false;
        for (uint32_t i = 0; i < per_sector; ++i) fat_.push_back(cerf::le::U32(bytes_.data(), off + 4u * i));
    }
    return true;
}

bool CompoundFile::Chain(const std::vector<uint32_t>& table, uint32_t start,
                         std::vector<uint32_t>& out) {
    out.clear();
    uint32_t sector = start;
    while (sector != kEndOfChain) {
        if (sector >= table.size()) return Fail("a chain runs to sector " + std::to_string(sector) +
                                                ", past its allocation table");
        if (out.size() >= table.size()) return Fail("a chain starting at " + std::to_string(start) + " loops");
        out.push_back(sector);
        sector = table[sector];
        if (sector == kFreeSect) return Fail("a chain starting at " + std::to_string(start) +
                                             " reaches a free sector");
    }
    return true;
}

bool CompoundFile::ReadDirectory() {
    std::vector<uint32_t> chain;
    if (!Chain(fat_, cerf::le::U32(bytes_.data(), kFirstDirOff), chain)) return false;
    for (uint32_t sector : chain) {
        size_t off = 0;
        if (!SectorOffset(sector, off)) return false;
        for (uint32_t at = 0; at + kEntryBytes <= sector_size_; at += kEntryBytes) {
            const uint8_t* e = bytes_.data() + off + at;
            Entry entry;
            const uint32_t name_bytes = cerf::le::U16(e, kEntryNameLen);
            if (name_bytes > kNameBytesMax || name_bytes % 2u != 0u)
                return Fail("directory entry " + std::to_string(entries_.size()) + " has a name length of " +
                            std::to_string(name_bytes));
            for (uint32_t i = 0; i + 2u < name_bytes; i += 2u)
                entry.name.push_back(static_cast<char16_t>(cerf::le::U16(e, i)));
            entry.type  = e[kEntryTypeOff];
            entry.left  = cerf::le::U32(e, kEntryLeftOff);
            entry.right = cerf::le::U32(e, kEntryRightOff);
            entry.child = cerf::le::U32(e, kEntryChildOff);
            entry.start = cerf::le::U32(e, kEntryStartOff);
            entry.size  = major_ == kMajorV3 ? cerf::le::U32(e, kEntrySizeOff) : cerf::le::U64(e, kEntrySizeOff);
            entries_.push_back(std::move(entry));
        }
    }
    if (entries_.empty() || entries_[0].type != kTypeRoot)
        return Fail("stream ID 0 is not the root storage");
    return true;
}

bool CompoundFile::ReadMiniStream() {
    const uint32_t first_mini_fat = cerf::le::U32(bytes_.data(), kFirstMiniFatOff);
    if (first_mini_fat != kEndOfChain) {
        std::vector<uint32_t> chain;
        if (!Chain(fat_, first_mini_fat, chain)) return false;
        for (uint32_t sector : chain) {
            size_t off = 0;
            if (!SectorOffset(sector, off)) return false;
            for (uint32_t i = 0; i < sector_size_ / 4u; ++i)
                mini_fat_.push_back(cerf::le::U32(bytes_.data(), off + 4u * i));
        }
    }
    const Entry& root = entries_[0];
    if (root.size == 0u) return true;
    std::vector<uint32_t> chain;
    if (!Chain(fat_, root.start, chain)) return false;
    if (uint64_t(chain.size()) * sector_size_ < root.size)
        return Fail("the mini stream chain is shorter than its " + std::to_string(root.size) + " bytes");
    for (uint32_t sector : chain) {
        size_t off = 0;
        if (!SectorOffset(sector, off)) return false;
        mini_stream_.insert(mini_stream_.end(), bytes_.begin() + off, bytes_.begin() + off + sector_size_);
    }
    mini_stream_.resize(static_cast<size_t>(root.size));
    return true;
}

bool CompoundFile::NameMatches(const std::u16string& name, const char* ascii) const {
    const size_t length = std::strlen(ascii);
    if (name.size() != length) return false;
    for (size_t i = 0; i < length; ++i) {
        char16_t a = name[i];
        char16_t b = static_cast<unsigned char>(ascii[i]);
        if (a >= u'a' && a <= u'z') a = static_cast<char16_t>(a - u'a' + u'A');
        if (b >= u'a' && b <= u'z') b = static_cast<char16_t>(b - u'a' + u'A');
        if (a != b) return false;
    }
    return true;
}

bool CompoundFile::FindChild(uint32_t parent, const char* name, uint32_t& found) {
    std::vector<uint32_t> pending{entries_[parent].child};
    std::vector<bool>     seen(entries_.size(), false);
    while (!pending.empty()) {
        const uint32_t id = pending.back();
        pending.pop_back();
        if (id == kNoStream) continue;
        if (id >= entries_.size() || seen[id]) return Fail("the directory tree under entry " +
                                                           std::to_string(parent) + " is not a tree");
        seen[id] = true;
        if (NameMatches(entries_[id].name, name)) {
            found = id;
            return true;
        }
        pending.push_back(entries_[id].left);
        pending.push_back(entries_[id].right);
    }
    return Fail(std::string("no entry named ") + name);
}

bool CompoundFile::ReadStream(std::initializer_list<const char*> path, std::vector<uint8_t>& out) {
    uint32_t id = 0;
    for (const char* name : path) {
        if (entries_[id].type != kTypeRoot && entries_[id].type != kTypeStorage)
            return Fail(std::string("a path component before ") + name + " is not a storage");
        if (!FindChild(id, name, id)) return false;
    }
    const Entry& entry = entries_[id];
    if (entry.type != kTypeStream) return Fail("the path does not name a stream");

    out.clear();
    out.reserve(static_cast<size_t>(entry.size));
    std::vector<uint32_t> chain;
    if (entry.size < kMiniCutoff) {
        if (!Chain(mini_fat_, entry.start, chain)) return false;
        for (uint32_t sector : chain) {
            const uint64_t at = uint64_t(sector) * kMiniSectorSize;
            if (at + kMiniSectorSize > mini_stream_.size())
                return Fail("mini sector " + std::to_string(sector) + " lies past the mini stream");
            const size_t off = static_cast<size_t>(at);
            out.insert(out.end(), mini_stream_.begin() + off, mini_stream_.begin() + off + kMiniSectorSize);
        }
    } else {
        if (!Chain(fat_, entry.start, chain)) return false;
        for (uint32_t sector : chain) {
            size_t off = 0;
            if (!SectorOffset(sector, off)) return false;
            out.insert(out.end(), bytes_.begin() + off, bytes_.begin() + off + sector_size_);
        }
    }
    if (out.size() < entry.size)
        return Fail("the stream chain is shorter than its " + std::to_string(entry.size) + " bytes");
    out.resize(static_cast<size_t>(entry.size));
    return true;
}

}
