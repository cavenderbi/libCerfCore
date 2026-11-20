#include <cerfcore/state/state_boot_gate.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/execution/jit_runner.h>
#include <cerfcore/platform/host_file.h>
#include <cerfcore/state/hibernation.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(StateBootGate);

void StateBootGate::OnReady() {
    emu_.Get<JitRunner>().RegisterPreStartHook([this] { Run(); });
}

void StateBootGate::RegisterColdStartListener(std::function<void()> fn) { cold_start_listeners_.Add(std::move(fn)); }

std::string StateBootGate::ImagePath() const {
    const DeviceConfig& config = emu_.Get<DeviceConfig>();
    if (config.state_path.empty()) return emu_.Get<Hibernation>().DefaultStatePath();
    return ResolveDeviceFile(config.device_name, config.state_path);
}

void StateBootGate::Run() {
    auto&               hib    = emu_.Get<Hibernation>();
    const DeviceConfig& config = emu_.Get<DeviceConfig>();
    const std::string   image  = ImagePath();
    const bool          found  = host_platform::HostPathExists(image);
    if (!found && !config.state_path.empty() && config.boot_mode != StateBootMode::Cold) {
        emu_.Get<Fatal>().UserError("The state image %s does not exist", image.c_str());
    }
    bool cold = true;
    if (found) {
        switch (config.boot_mode) {
            case StateBootMode::Cold:   break;
            case StateBootMode::Warm:   cold = !hib.Restore(image, true); break;
            case StateBootMode::Resume: cold = !hib.Restore(image, false); break;
        }
    }
    if (cold) cold_start_listeners_.Call();
}

}
