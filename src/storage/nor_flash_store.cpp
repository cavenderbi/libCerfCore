#include "nor_flash_store.h"

#include "../boards/page_table_builder.h"
#include <cerfcore/boards/board_context.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/device_config_keys.h>
#include <cerfcore/core/device_file_types.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/host_file_bytes.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/platform/host_error.h>
#include <cerfcore/state/state_stream.h>

#include <utility>
#include <vector>

namespace cerfcore {

REGISTER_SERVICE(NorFlashStore);

std::string NorFlashStore::ImagePath() const {
    const DeviceConfig& cfg = emu_.Get<DeviceConfig>();
    return ResolveDeviceFile(cfg.device_name, cfg.storage_nor);
}

bool NorFlashStore::ShouldRegister() {
    for (const DeviceFileType& type :
         emu_.Get<DeviceFileTypes>().ForBoard(emu_.Get<BoardContext>().GetBoardId())) {
        if (type.section == DeviceFileSection::kStorage && type.id == config_key::kStorageNor)
            return HostFileNonEmpty(ImagePath());
    }
    return false;
}

void NorFlashStore::OnReady() {
    const std::string path = ImagePath();
    if (!file_.Open(path, host_platform::FileMode::kReadWriteExisting))
        emu_.Get<Fatal>().UserError("%s: cannot open '%s' (error %u)", kObjectName, path.c_str(),
                                    host_platform::LastErrorCode());
    emu_.Get<MachineObjectTree>().Add(this);
    LOG(Boot, "NorFlashStore: image '%s'\n", path.c_str());
    for (const auto& region : emu_.Get<PageTableBuilder>().BackedMemoryRegions()) {
        if (region.kind != MemoryKind::kStorage) continue;
        Load(emu_.Get<EmulatedMemory>().Translate(region.pa_base), region.size);
        pa_base_ = region.pa_base;
        return;
    }
}

void NorFlashStore::Load(uint8_t* array, uint32_t size) {
    const std::string path  = ImagePath();
    uint64_t          bytes = 0;
    if (!file_.Size(bytes))
        emu_.Get<Fatal>().UserError("%s: cannot read the size of '%s'", kObjectName, path.c_str());
    if (bytes != size)
        emu_.Get<Fatal>().UserError("%s: '%s' holds %llu bytes; the flash array holds %u bytes",
                                    kObjectName, path.c_str(), static_cast<unsigned long long>(bytes),
                                    size);
    uint32_t done = 0;
    if (!file_.SeekTo(0) || !file_.Read(array, size, done) || done != size)
        emu_.Get<Fatal>().UserError("%s: read of '%s' failed", kObjectName, path.c_str());
    array_ = array;
    size_  = size;
}

void NorFlashStore::Overlay(uint32_t offset, const uint8_t* bytes, uint32_t length) {
    for (uint32_t i = 0; i < length; ++i) {
        OverlayByte& entry = overlay_[offset + i];
        entry.injected = bytes[i];
        SetOverlayActive(offset + i, entry, true);
    }
}

void NorFlashStore::DropOverlay(uint32_t offset, uint32_t length) {
    for (auto it = overlay_.lower_bound(offset);
         it != overlay_.end() && it->first < uint64_t(offset) + length; ++it)
        SetOverlayActive(it->first, it->second, false);
}

void NorFlashStore::SetOverlayActive(uint32_t offset, OverlayByte& entry, bool active) {
    if (active) {
        if (!entry.active) entry.pristine = array_[offset];
        array_[offset] = entry.injected;
    } else if (entry.active) {
        array_[offset] = entry.pristine;
    }
    entry.active = active;
}

void NorFlashStore::SaveState(StateWriter& w) const {
    std::vector<std::pair<uint32_t, uint32_t>> runs;
    for (const auto& [offset, entry] : overlay_) {
        if (!entry.active) continue;
        if (!runs.empty() && runs.back().first + runs.back().second == offset)
            ++runs.back().second;
        else
            runs.push_back({offset, 1u});
    }
    w.Write<uint64_t>("overlay_run_count", runs.size());
    for (const auto& [offset, length] : runs) {
        w.Write("overlay_run_offset", offset);
        w.Write("overlay_run_length", length);
    }
}

void NorFlashStore::RestoreState(StateReader& r) {
    uint64_t count = 0;
    r.Read("overlay_run_count", count);
    std::vector<std::pair<uint32_t, uint32_t>> runs;
    for (uint64_t i = 0; i < count; ++i) {
        uint32_t offset = 0, length = 0;
        r.Read("overlay_run_offset", offset);
        r.Read("overlay_run_length", length);
        runs.push_back({offset, length});
    }
    size_t run = 0;
    for (auto& [offset, entry] : overlay_) {
        while (run < runs.size() && uint64_t(runs[run].first) + runs[run].second <= offset) ++run;
        SetOverlayActive(offset, entry, run < runs.size() && runs[run].first <= offset);
    }
}

void NorFlashStore::Store(uint32_t offset, const uint8_t* bytes, uint32_t length) {
    uint32_t done = 0;
    if (!file_.SeekTo(offset) || !file_.Write(bytes, length, done) || done != length)
        emu_.Get<Fatal>().UserError("%s: write of %u bytes at offset 0x%08X to '%s' failed",
                                    kObjectName, length, offset, ImagePath().c_str());
    MarkTx();
}

}
