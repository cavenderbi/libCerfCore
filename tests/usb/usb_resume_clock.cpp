#include "../../src/execution/guest_cycle_clock.h"
#include "../../src/socs/imx51/imx51_usb_device_transfers.h"
#include "../../src/socs/imx51/imx51_usb_transceiver_clocks.h"
#include "../../src/socs/irq_controller.h"
#include "../../src/peripherals/peripheral_dispatcher.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/boards/board_context.h>
#include <stdexcept>

namespace cerfcore {
void GuestCycleClock::OnReady() { rate_ = InitialRate(); }
GuestCycleClock::Event* GuestCycleClock::Add(std::function<void()> fn) {
    events_.push_back(std::make_unique<Event>(std::move(fn)));
    return events_.back().get();
}
void GuestCycleClock::Arm(Event* event, uint64_t at) { event->at_ = at; event->armed_ = true; }
void GuestCycleClock::Disarm(Event* event) { event->armed_ = false; }
bool GuestCycleClock::IsDue(const Event* event, uint64_t now) const {
    return event->armed_ && event->at_ <= now;
}
uint64_t GuestCycleClock::NsToCycles(int64_t ns) const { return ns * rate_.num / 1000000000u; }
int64_t GuestCycleClock::CyclesToNs(uint64_t cycles) const { return cycles * 1000000000u / rate_.num; }
void GuestCycleClock::OnDispatch() {
    for (auto& event : events_) {
        if (!IsDue(event.get(), Cycles())) continue;
        event->armed_ = false;
        event->fn_();
    }
}
void GuestCycleClock::RegisterRateListener(std::function<void()> fn) { rate_listeners_.push_back(std::move(fn)); }
void GuestCycleClock::SetClockRate(Rate rate) {
    guest_ns_ = NowNs();
    guest_ns_cycle_ = Cycles();
    rate_ = rate;
    for (auto& listener : rate_listeners_) listener();
}
void GuestCycleClock::IdleStep(void*) { throw std::runtime_error("unexpected clock idle"); }
void GuestCycleClock::WaitStep(void*) { throw std::runtime_error("unexpected clock wait"); }
void GuestCycleClock::ExitIdle() {}
namespace host_platform { HostWaitTimer::~HostWaitTimer() = default; }
bool Imx51UsbDeviceTransfers::ShouldRegister() { return true; }
bool Imx51UsbDeviceTransfers::WriteSetup(uint32_t, const uint8_t*) { throw std::runtime_error("device transfer"); }
uint32_t Imx51UsbDeviceTransfers::ExecutePrime(uint32_t, uint32_t) { throw std::runtime_error("device transfer"); }
bool Imx51UsbTransceiverClocks::ShouldRegister() { return true; }
void Imx51UsbTransceiverClocks::OnReady() {}
void Imx51UsbTransceiverClocks::RegisterChangeListener(std::function<void(FreescaleLowPowerMode)>) {}
bool Imx51UsbTransceiverClocks::CoreClockRuns(uint32_t, uint32_t, uint32_t, uint32_t,
                                           FreescaleLowPowerMode) const { return false; }
void IrqController::PulseIrq(int source) { AssertIrq(source); DeAssertIrq(source); }
uint32_t IrqController::ReadPendingVector() { return 0; }
void PeripheralDispatcher::Register(Peripheral*) { throw std::runtime_error("controller OnReady"); }
void GuestCpuReset::RegisterResetListener(std::function<void(ResetLineKind)>) {}
std::string_view BoardContext::GetSocId() const { throw std::runtime_error("board lookup"); }
EmulatedMemory::~EmulatedMemory() = default;
void EmulatedMemory::OnReady() { throw std::runtime_error("memory initialization"); }
uint8_t* EmulatedMemory::TryTranslate(uint32_t) { throw std::runtime_error("memory translation"); }
uint32_t EmulatedMemory::ReadWord(uint32_t) { throw std::runtime_error("memory read"); }
void EmulatedMemory::WriteWord(uint32_t, uint32_t) { throw std::runtime_error("memory write"); }
void EmulatedMemory::CopyIn(uint32_t, const void*, size_t) { throw std::runtime_error("memory copy"); }
void EmulatedMemory::CopyOut(uint32_t, void*, size_t) { throw std::runtime_error("memory copy"); }
}
