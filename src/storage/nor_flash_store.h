#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_storage.h>
#include <cerfcore/platform/host_file.h>

#include <cstdint>
#include <map>
#include <span>
#include <string>

namespace cerfcore {

class StateReader;
class StateWriter;

class NorFlashStore : public Service, public MachineStorage {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void Store(uint32_t offset, const uint8_t* bytes, uint32_t length);

    std::span<const uint8_t> Array() const { return {array_, size_}; }
    uint32_t                 PaBase() const { return pa_base_; }
    void Overlay(uint32_t offset, const uint8_t* bytes, uint32_t length);
    void DropOverlay(uint32_t offset, uint32_t length);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

    std::string   ObjectId() const override { return "nor_flash"; }
    static constexpr char kObjectName[] = "NOR Flash";
    std::string   ObjectName() const override { return kObjectName; }
    StorageMedium Medium() const override { return StorageMedium::kNorFlash; }
    std::string   ImagePath() const override;

private:
    struct OverlayByte {
        uint8_t injected = 0;
        uint8_t pristine = 0;
        bool    active   = false;
    };

    void Load(uint8_t* array, uint32_t size);
    void SetOverlayActive(uint32_t offset, OverlayByte& entry, bool active);

    host_platform::HostFile         file_;
    uint32_t                        pa_base_ = 0;
    uint8_t*                        array_   = nullptr;
    uint32_t                        size_    = 0;
    std::map<uint32_t, OverlayByte> overlay_;
};

}
