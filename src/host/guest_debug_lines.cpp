#include <cerfcore/host/guest_debug_lines.h>

#include <cerfcore/core/cerf_emulator.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(GuestDebugLines);

void GuestDebugLines::Subscribe(Listener listener) { listeners_.Add(std::move(listener)); }

void GuestDebugLines::Publish(std::string_view line, std::string_view source) { listeners_.Call(line, source); }

}
