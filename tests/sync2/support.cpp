// A minimal locator and a recording UART peer. Protocol, controller, signal and
// state-file implementations are production code; no ROM or frontend is needed.
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/fatal.h>
#include "../../src/boards/ford_sync_gen2/ford_sync_gen2_vmcu_peer.h"
#include <stdexcept>

std::vector<std::vector<uint8_t>> packets;
namespace cerfcore {
CerfEmulator::CerfEmulator(const CerfConfig& config, int argc, char** argv)
    : config_(config), argc_(argc), argv_(argv) {}
CerfEmulator::~CerfEmulator() { TearDown(); }
void CerfEmulator::OnServiceDestroy(const servicelocator::Service&) {}
void CerfEmulator::ShutDownService(servicelocator::Service& service) { service.OnShutdown(); }
void CerfEmulator::RaiseLocatorError(const std::string& message) { throw std::runtime_error(message); }
bool BoardContext::ShouldRegister() { return true; }
void BoardContext::OnReady() {}
bool FordSyncGen2VmcuPeer::ShouldRegister() { return true; }
void FordSyncGen2VmcuPeer::OnReady() {}
void FordSyncGen2VmcuPeer::OnGuestTx(uint8_t) {}
void FordSyncGen2VmcuPeer::SaveState(StateWriter&) {}
void FordSyncGen2VmcuPeer::RestoreState(StateReader&) {}
void FordSyncGen2VmcuPeer::InjectReliable(uint8_t, uint8_t, const uint8_t* p, std::size_t n) {
    packets.emplace_back(p, p + n);
}
void Fatal::OnReady() {}
void Fatal::Die(const char*, ...) { throw std::runtime_error("unexpected fatal error"); }
namespace Log {
namespace detail { std::atomic<uint64_t> enabled_mask{0}; }
void Print(Cat, const char*, ...) {}
}
}
