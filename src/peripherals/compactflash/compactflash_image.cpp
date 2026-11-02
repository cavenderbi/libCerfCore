#include "compactflash_image.h"

namespace cerfcore {

CompactFlashImage::CompactFlashImage(const std::string& path) {
    uint64_t bytes = 0;
    if (file_.Open(path, host_platform::FileMode::kReadWriteExisting))
        file_.Size(bytes);
    total_sectors_ = bytes / kSectorBytes;
}

bool CompactFlashImage::ReadSector(uint64_t lba, uint8_t* out) {
    if (!file_.IsOpen() || lba >= total_sectors_)
        return false;
    if (!file_.SeekTo(lba * kSectorBytes))
        return false;
    uint32_t got = 0;
    return file_.Read(out, kSectorBytes, got) && got == kSectorBytes;
}

bool CompactFlashImage::WriteSector(uint64_t lba, const uint8_t* in) {
    if (!file_.IsOpen() || lba >= total_sectors_)
        return false;
    if (!file_.SeekTo(lba * kSectorBytes))
        return false;
    uint32_t put = 0;
    return file_.Write(in, kSectorBytes, put) && put == kSectorBytes;
}

}
