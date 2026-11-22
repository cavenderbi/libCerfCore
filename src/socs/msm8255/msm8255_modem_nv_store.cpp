#include "msm8255_modem_nv_store.h"

#include "../../storage/compound_file.h"

#include <cerfcore/boards/board_context.h>
#include "msm8255_id.h"
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/host_file_bytes.h>
#include <cerfcore/core/log.h>
#include <cerfcore/platform/host_file.h>

#include <cstring>
#include <string_view>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint32_t kRecordBytes     = 136u;
constexpr uint32_t kRecordLengthOff = 0u;
constexpr uint32_t kRecordTagOff    = 2u;
constexpr uint32_t kRecordItemOff   = 4u;
constexpr uint32_t kRecordIndexOff  = 6u;
constexpr uint32_t kRecordDataOff   = 8u;
constexpr uint16_t kRecordTag       = 1u;
constexpr uint16_t kUnindexed       = 0u;

constexpr size_t kNoRecord = static_cast<size_t>(-1);

}

bool Msm8255ModemNvStore::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetSocId() == SocId::Msm8255;
}

void Msm8255ModemNvStore::OnReady() {
    const DeviceConfig& cfg = emu_.Get<DeviceConfig>();
    path_ = ResolveDeviceFile(cfg.device_name, cfg.storage_modem_nv);
    if (!HostFileNonEmpty(path_)) {
        Seed();
        return;
    }
    std::vector<uint8_t> bytes;
    if (!TryReadHostFileBytes(path_, bytes)) {
        emu_.Get<Fatal>().UserError("storage.modem_nv %s cannot be read", path_.c_str());
    }
    Load(std::move(bytes), "storage.modem_nv " + path_);
    LOG(Boot, "modem nv: using storage.modem_nv %s, %zu items\n", path_.c_str(),
        records_.size() / kRecordBytes);
}

void Msm8255ModemNvStore::Seed() {
    const DeviceConfig& cfg = emu_.Get<DeviceConfig>();
    if (cfg.rom_qcn.empty()) {
        LOG(Boot, "modem nv: no rom.qcn, storage.modem_nv %s starts empty\n", path_.c_str());
        return;
    }
    const std::string qcn = ResolveDeviceFile(cfg.device_name, cfg.rom_qcn);
    std::vector<uint8_t> bytes;
    if (!TryReadHostFileBytes(qcn, bytes)) {
        emu_.Get<Fatal>().UserError("rom.qcn %s cannot be read", qcn.c_str());
    }
    CompoundFile file;
    std::vector<uint8_t> records;
    if (!file.Parse(std::move(bytes)) ||
        !file.ReadStream({"00000000", "default", "NV_NUMBERED_ITEMS", "NV_ITEM_ARRAY"}, records)) {
        emu_.Get<Fatal>().UserError("rom.qcn %s: %s", qcn.c_str(), file.Error().c_str());
    }
    Load(std::move(records), "rom.qcn " + qcn);
    Persist();
    LOG(Boot, "modem nv: storage.modem_nv %s seeded from rom.qcn %s, %zu items\n", path_.c_str(),
        qcn.c_str(), records_.size() / kRecordBytes);
}

void Msm8255ModemNvStore::Load(std::vector<uint8_t> records, const std::string& source) {
    if (records.size() % kRecordBytes != 0u) {
        emu_.Get<Fatal>().UserError("%s: the NV item array is %zu bytes, not whole %u-byte records",
                                    source.c_str(), records.size(), kRecordBytes);
    }
    records_.clear();
    for (size_t at = 0; at < records.size(); at += kRecordBytes) {
        const uint8_t* r      = records.data() + at;
        const uint16_t length = cerf::le::U16(r, kRecordLengthOff);
        const uint16_t tag    = cerf::le::U16(r, kRecordTagOff);
        const uint16_t item   = cerf::le::U16(r, kRecordItemOff);
        const uint16_t index  = cerf::le::U16(r, kRecordIndexOff);
        if (length != kRecordBytes || tag != kRecordTag) {
            emu_.Get<Fatal>().UserError("%s: record %zu of item %u has length %u and tag %u",
                                        source.c_str(), at / kRecordBytes, item, length, tag);
        }
        if (RecordOffset(item, index) != kNoRecord) {
            emu_.Get<Fatal>().UserError("%s: item %u index %u appears twice", source.c_str(), item, index);
        }
        records_.insert(records_.end(), r, r + kRecordBytes);
    }
}

void Msm8255ModemNvStore::Persist() {
    uint32_t error = 0;
    const std::string_view bytes(reinterpret_cast<const char*>(records_.data()), records_.size());
    if (!host_platform::ReplaceHostFileContents(path_, bytes, true, error)) {
        emu_.Get<Fatal>().UserError("storage.modem_nv %s cannot be written (error %u)", path_.c_str(), error);
    }
}

size_t Msm8255ModemNvStore::RecordOffset(uint32_t item, uint32_t index) const {
    for (size_t at = 0; at < records_.size(); at += kRecordBytes) {
        if (cerf::le::U16(records_.data(), at + kRecordItemOff) == item &&
            cerf::le::U16(records_.data(), at + kRecordIndexOff) == index) {
            return at;
        }
    }
    return kNoRecord;
}

bool Msm8255ModemNvStore::Find(uint32_t item, uint8_t (&data)[kModemNvItemDataBytes]) const {
    const size_t at = RecordOffset(item, kUnindexed);
    if (at == kNoRecord) return false;
    std::memcpy(data, records_.data() + at + kRecordDataOff, kModemNvItemDataBytes);
    return true;
}

bool Msm8255ModemNvStore::HoldsAnyIndex(uint32_t item) const {
    for (size_t at = 0; at < records_.size(); at += kRecordBytes) {
        if (cerf::le::U16(records_.data(), at + kRecordItemOff) == item) return true;
    }
    return false;
}

void Msm8255ModemNvStore::Store(uint32_t item, const uint8_t (&data)[kModemNvItemDataBytes]) {
    size_t at = RecordOffset(item, kUnindexed);
    if (at == kNoRecord) {
        at = records_.size();
        records_.resize(at + kRecordBytes, 0u);
        cerf::le::Put16(records_.data() + at + kRecordLengthOff, static_cast<uint16_t>(kRecordBytes));
        cerf::le::Put16(records_.data() + at + kRecordTagOff, kRecordTag);
        cerf::le::Put16(records_.data() + at + kRecordItemOff, static_cast<uint16_t>(item));
        cerf::le::Put16(records_.data() + at + kRecordIndexOff, kUnindexed);
    }
    std::memcpy(records_.data() + at + kRecordDataOff, data, kModemNvItemDataBytes);
    Persist();
}

REGISTER_SERVICE(Msm8255ModemNvStore);

}
