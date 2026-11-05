#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/fatal.h>
#include <stdexcept>

namespace cerfcore {
CerfEmulator::CerfEmulator(const CerfConfig& config, int argc, char** argv)
    : config_(config), argc_(argc), argv_(argv) {}
CerfEmulator::~CerfEmulator() { TearDown(); }
void CerfEmulator::OnServiceDestroy(const servicelocator::Service&) {}
void CerfEmulator::ShutDownService(servicelocator::Service& service) { service.OnShutdown(); }
void CerfEmulator::RaiseLocatorError(const std::string& message) { throw std::runtime_error(message); }
bool BoardContext::ShouldRegister() { return true; }
void BoardContext::OnReady() {}
void Fatal::OnReady() {}
void Fatal::Die(const char*, ...) { throw std::runtime_error("unexpected fatal error"); }
namespace Log {
namespace detail { std::atomic<uint64_t> enabled_mask{0}; }
void Print(Cat, const char*, ...) {}
}
}
