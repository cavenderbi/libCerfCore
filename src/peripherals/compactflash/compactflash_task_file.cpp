#include "compactflash_task_file.h"

#include <cerfcore/state/state_stream.h>

#include <algorithm>

namespace cerfcore {

namespace {

constexpr uint32_t kMaxCylinders = 0xFFFFu;

}

void CompactFlashTaskFile::PowerOn() {
    ApplySignature();
    status   = 0;
    dev_ctrl = 0;
    geometry = kDefaultGeometry;
}

void CompactFlashTaskFile::ApplySignature() {
    feature  = 0;
    error    = kDiagnosticNoError;
    sect_cnt = 1;
    sect_num = 1;
    cyl_low  = 0;
    cyl_high = 0;
    drv_head = 0xA0;
}

uint32_t CompactFlashTaskFile::Cylinders(CompactFlashGeometry translation, uint64_t total_sectors) {
    const uint64_t per_cylinder = static_cast<uint64_t>(translation.heads) * translation.sectors;
    if (per_cylinder == 0u)
        return 0u;
    return static_cast<uint32_t>(std::min<uint64_t>(total_sectors / per_cylinder, kMaxCylinders));
}

uint64_t CompactFlashTaskFile::AddressLba() const {
    if ((drv_head & kHeadLba) != 0u) {
        return (static_cast<uint64_t>(Head()) << 24) | (static_cast<uint64_t>(cyl_high) << 16) |
               (static_cast<uint64_t>(cyl_low) << 8) | sect_num;
    }
    return (static_cast<uint64_t>(Cylinder()) * geometry.heads + Head()) * geometry.sectors +
           (sect_num - 1u);
}

CompactFlashAddressFault CompactFlashTaskFile::Resolve(uint64_t total_sectors, uint64_t* lba) const {
    if ((drv_head & kHeadLba) == 0u) {
        if (sect_num == 0u || sect_num > geometry.sectors || Head() >= geometry.heads)
            return CompactFlashAddressFault::kInvalidAddress;
        if (Cylinder() >= Cylinders(total_sectors))
            return CompactFlashAddressFault::kAddressTooLarge;
    }
    const uint64_t address = AddressLba();
    if (address >= total_sectors)
        return CompactFlashAddressFault::kAddressTooLarge;
    *lba = address;
    return CompactFlashAddressFault::kNone;
}

CompactFlashAddressFault CompactFlashTaskFile::ResolveCylinderHead(uint64_t total_sectors,
                                                                    uint64_t* track_lba) const {
    if ((drv_head & kHeadLba) != 0u)
        return Resolve(total_sectors, track_lba);
    if (Head() >= geometry.heads)
        return CompactFlashAddressFault::kInvalidAddress;
    if (Cylinder() >= Cylinders(total_sectors))
        return CompactFlashAddressFault::kAddressTooLarge;
    *track_lba = (static_cast<uint64_t>(Cylinder()) * geometry.heads + Head()) * geometry.sectors;
    return CompactFlashAddressFault::kNone;
}

void CompactFlashTaskFile::Advance() {
    SetAddress(AddressLba() + 1u);
}

void CompactFlashTaskFile::SetAddress(uint64_t lba) {
    if ((drv_head & kHeadLba) != 0u) {
        sect_num = static_cast<uint8_t>(lba & 0xFFu);
        cyl_low  = static_cast<uint8_t>((lba >> 8) & 0xFFu);
        cyl_high = static_cast<uint8_t>((lba >> 16) & 0xFFu);
        drv_head = static_cast<uint8_t>((drv_head & 0xF0u) | ((lba >> 24) & 0x0Fu));
        return;
    }
    const uint64_t track    = lba / geometry.sectors;
    const uint64_t cylinder = track / geometry.heads;
    sect_num = static_cast<uint8_t>(lba % geometry.sectors + 1u);
    cyl_low  = static_cast<uint8_t>(cylinder & 0xFFu);
    cyl_high = static_cast<uint8_t>((cylinder >> 8) & 0xFFu);
    drv_head = static_cast<uint8_t>((drv_head & 0xF0u) | (track % geometry.heads));
}

void CompactFlashTaskFile::SaveState(StateWriter& w) const {
    w.Write("feature", feature);
    w.Write("error", error);
    w.Write("sect_cnt", sect_cnt);
    w.Write("sect_num", sect_num);
    w.Write("cyl_low", cyl_low);
    w.Write("cyl_high", cyl_high);
    w.Write("drv_head", drv_head);
    w.Write("status", status);
    w.Write("dev_ctrl", dev_ctrl);
    w.Write("chs_heads", geometry.heads);
    w.Write("chs_sectors", geometry.sectors);
}

void CompactFlashTaskFile::RestoreState(StateReader& r) {
    r.Read("feature", feature);
    r.Read("error", error);
    r.Read("sect_cnt", sect_cnt);
    r.Read("sect_num", sect_num);
    r.Read("cyl_low", cyl_low);
    r.Read("cyl_high", cyl_high);
    r.Read("drv_head", drv_head);
    r.Read("status", status);
    r.Read("dev_ctrl", dev_ctrl);
    r.Read("chs_heads", geometry.heads);
    r.Read("chs_sectors", geometry.sectors);
}

}
