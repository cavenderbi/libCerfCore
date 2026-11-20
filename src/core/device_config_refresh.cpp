#include <cerfcore/core/device_config_refresh.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/config_json.h>
#include <cerfcore/core/config_reader.h>
#include "config_loader.h"
#include "config_mutable_fields.h"
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/log.h>

namespace cerfcore {

REGISTER_SERVICE(DeviceConfigRefresh);

void DeviceConfigRefresh::RegisterListener(std::function<void()> fn) {
    listeners_.Add(std::move(fn));
}

LiveDeviceSettings DeviceConfigRefresh::LoadFiles(const DeviceConfig& config) {
    LiveDeviceSettings loaded;
    static const std::pair<const char*, CfgBrokenFile> kFiles[] = {
        {kDeviceConfigFileName, CfgBrokenFile::kHalt}, {kUserConfigFileName, CfgBrokenFile::kSkip}};
    for (const auto& [name, broken] : kFiles) {
        const std::string path = ResolveDeviceFile(config.device_name, name);
        nlohmann::json j = emu_.Get<ConfigReader>().ReadLoggedJsonFile(path, broken);
        if (j.is_null()) continue;
        emu_.Get<ConfigMutableFields>().LoadAll(j, loaded, path);
    }
    emu_.Get<ConfigLoader>().ApplyBoardPanelDefault(config.board_id, loaded);
    return loaded;
}

void DeviceConfigRefresh::Refresh() {
    auto&    config     = emu_.Get<DeviceConfig>();
    bool published = false;
    while (!published) {
        const uint64_t generation = config.LiveGeneration();
        published                 = config.PublishLiveAt(generation, LoadFiles(config));
    }

    const auto live = config.Live();
    LOG(Cfg, "DeviceConfigRefresh: %ux%u dpi=%u bpp=%u palette_colors=%zu font=%d(set=%d) cleartype=%d(set=%d) untrusted=%d scheme='%s' share='%s' mount='%s'\n",
        live->board_configurable_screen_width,
        live->board_configurable_screen_height, live->screen_dpi,
        live->board_configurable_screen_bpp,
        live->guest_additions_palette.size(),
        live->guest_additions_font_size,
        live->guest_additions_font_size_set ? 1 : 0,
        live->guest_additions_cleartype ? 1 : 0,
        live->guest_additions_cleartype_set ? 1 : 0,
        live->guest_additions_allow_untrusted_apps ? 1 : 0,
        live->guest_additions_color_scheme.c_str(),
        live->share_folder.c_str(),
        live->share_folder_mount_point.c_str());

    listeners_.Call();
}

}
