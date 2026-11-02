#pragma once

#include <cerfcore/platform/host_file.h>

#include <cstdint>
#include <string>

namespace cerfcore {

class CompactFlashImage {
public:
    static constexpr uint32_t kSectorBytes = 512u;

    explicit CompactFlashImage(const std::string& path);

    bool     IsOpen() const { return file_.IsOpen(); }
    uint64_t TotalSectors() const { return total_sectors_; }

    bool ReadSector(uint64_t lba, uint8_t* out);
    bool WriteSector(uint64_t lba, const uint8_t* in);

private:
    host_platform::HostFile file_;
    uint64_t                total_sectors_ = 0;
};

}
