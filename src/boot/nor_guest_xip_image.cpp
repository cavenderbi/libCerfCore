#include "guest_xip_image.h"

#include "rom_image_parse.h"
#include "rom_toc_lookup.h"
#include "../storage/nor_flash_store.h"
#include <cerfcore/core/board_database.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/log.h>

#include <string>

namespace cerfcore {

namespace {

constexpr const char* kKernelModule = "nk.exe";

class NorGuestXipImage final : public GuestXipImage {
public:
    using GuestXipImage::GuestXipImage;

    bool ShouldRegister() override {
        return emu_.Get<DeviceConfig>().guest_additions && emu_.TryGet<NorFlashStore>() != nullptr;
    }

    void OnReady() override {
        const std::span<const uint8_t> array   = emu_.Get<NorFlashStore>().Array();
        const auto                     victims = emu_.Get<BoardDatabase>().GaVideoDriverNames();
        for (const auto& ecec : cerf::rom_image_parse::FindAllEcec(array)) {
            ParsedXipRegion xip;
            if (!cerf::rom_image_parse::ParseXipAtEcec(array, ecec, 0u, xip)) continue;
            if (!HoldsVictim(xip.toc, victims)) continue;
            xip_   = std::move(xip);
            found_ = true;
            LOG(GuestAdditions, "NOR XIP: ROMHDR 0x%08X physfirst=0x%08X physlast=0x%08X "
                                "modules=%zu\n", xip_.toc.romhdr_va, xip_.toc.romhdr.physfirst,
                xip_.toc.romhdr.physlast, xip_.toc.modules.size());
            return;
        }
        LOG(GuestAdditions, "NOR XIP: no XIP in the NOR image holds a display-driver victim\n");
    }

    const ParsedXipRegion* Primary() override { return found_ ? &xip_ : nullptr; }

    const ParsedTOCentry* FindModule(const char* name) override {
        return found_ ? cerf::rom_toc::FindModule(xip_.toc, name) : nullptr;
    }

    bool KernelSubsystemVersion(uint16_t& major, uint16_t& minor) override {
        const ParsedTOCentry* nk = FindModule(kKernelModule);
        return nk && cerf::rom_toc::ReadSubsystemVersion(emu_.Get<NorFlashStore>().Array(), xip_,
                                                         *nk, major, minor);
    }

    void Patch(uint32_t pa, std::span<const uint8_t> bytes) override {
        if (InArray(pa, uint32_t(bytes.size()))) {
            auto& store = emu_.Get<NorFlashStore>();
            store.Overlay(pa - store.PaBase(), bytes.data(), uint32_t(bytes.size()));
            return;
        }
        PatchRam(pa, bytes);
    }

private:
    bool InArray(uint32_t pa, uint32_t length) {
        auto&          store = emu_.Get<NorFlashStore>();
        const uint64_t end   = uint64_t(store.PaBase()) + store.Array().size();
        return pa >= store.PaBase() && uint64_t(pa) + length <= end;
    }

    static bool HoldsVictim(const ParsedTOC& toc, std::span<const std::string> victims) {
        for (const auto& victim : victims)
            if (cerf::rom_toc::FindModule(toc, victim)) return true;
        return false;
    }

    ParsedXipRegion xip_;
    bool            found_ = false;
};

}

REGISTER_SERVICE_AS(NorGuestXipImage, GuestXipImage);

}
