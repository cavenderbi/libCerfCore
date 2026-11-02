#pragma once

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

struct CompactFlashGeometry {
    uint32_t heads   = 0;
    uint32_t sectors = 0;
};

enum class CompactFlashAddressFault : uint8_t { kNone, kInvalidAddress, kAddressTooLarge };

class CompactFlashTaskFile {
public:
    static constexpr CompactFlashGeometry kDefaultGeometry{16u, 63u};
    static constexpr uint8_t kHeadLba           = 0x40u;
    static constexpr uint8_t kHeadDrive         = 0x10u;
    static constexpr uint8_t kDiagnosticNoError = 0x01u;
    static constexpr uint8_t kStatusBusy        = 0x80u;
    static constexpr uint8_t kStatusReady       = 0x40u;
    static constexpr uint8_t kStatusSeek        = 0x10u;
    static constexpr uint8_t kStatusDrq         = 0x08u;
    static constexpr uint8_t kStatusError       = 0x01u;
    static constexpr uint8_t kErrorIdnf         = 0x10u;
    static constexpr uint8_t kErrorAbort        = 0x04u;
    static constexpr uint8_t kDevCtlSoftReset   = 0x04u;
    static constexpr uint8_t kDevCtlNoIrq       = 0x02u;

    void PowerOn();
    void ApplySignature();
    uint8_t DriveSelect() const { return (drv_head & kHeadDrive) != 0u ? uint8_t{1} : uint8_t{0}; }

    static uint32_t Cylinders(CompactFlashGeometry translation, uint64_t total_sectors);
    uint32_t Cylinders(uint64_t total_sectors) const { return Cylinders(geometry, total_sectors); }
    CompactFlashAddressFault Resolve(uint64_t total_sectors, uint64_t* lba) const;
    CompactFlashAddressFault ResolveCylinderHead(uint64_t total_sectors, uint64_t* track_lba) const;
    void Advance();
    void SetAddress(uint64_t lba);
    uint32_t SectorCount() const { return sect_cnt == 0u ? 256u : sect_cnt; }

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

    uint8_t feature  = 0;
    uint8_t error    = 0;
    uint8_t sect_cnt = 1;
    uint8_t sect_num = 1;
    uint8_t cyl_low  = 0;
    uint8_t cyl_high = 0;
    uint8_t drv_head = 0xA0;
    uint8_t status   = 0;
    uint8_t dev_ctrl = 0;
    CompactFlashGeometry geometry = kDefaultGeometry;

private:
    uint32_t Cylinder() const { return (static_cast<uint32_t>(cyl_high) << 8) | cyl_low; }
    uint32_t Head() const { return drv_head & 0x0Fu; }
    uint64_t AddressLba() const;
};

}
