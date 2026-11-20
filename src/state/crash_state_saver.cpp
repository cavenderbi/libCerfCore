#include <cerfcore/state/crash_state_saver.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/execution/jit_runner.h>
#include <cerfcore/state/hibernation.h>
#include <cerfcore/state/state_image_format.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(CrashStateSaver);

void CrashStateSaver::OnReady() {
    emu_.Get<JitRunner>().RegisterRunEndListener([this] { SaveIfHalted(); });
}

std::string CrashStateSaver::Path() const {
    return ResolveDeviceFile(emu_.Get<DeviceConfig>().device_name, kCrashStateFile);
}

void CrashStateSaver::RegisterSavedListener(std::function<void(const std::string& path)> fn) {
    saved_listeners_.Add(std::move(fn));
}

void CrashStateSaver::SaveIfHalted() {
    if (!enabled_.load(std::memory_order_acquire) || !emu_.Get<Fatal>().Halted()) return;
    const std::string path = Path();
    std::string       error;
    if (!emu_.Get<Hibernation>().SaveAfterHalt(path, error)) {
        LOG(Caution, "CrashStateSaver: no crash state in %s: %s\n", path.c_str(), error.c_str());
        return;
    }
    LOG(Cerf, "CrashStateSaver: the halted machine is in %s\n", path.c_str());
    saved_listeners_.Call(path);
}

}
