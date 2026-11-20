#include "sh77xx_bsc.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/hibernation.h>
#include "../../peripherals/peripheral_dispatcher.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kArea4        = 4u;
constexpr uint32_t kArea4Base    = 0x10000000u;
constexpr uint32_t kArea4Last    = 0x13FFFFFFu;
constexpr uint32_t kWordBusBytes = 2u;

class Sh77xxArea4BusPort : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override { return emu_.TryGet<Sh77xxBsc>() != nullptr; }

    void OnReady() override {
        bsc_        = &emu_.Get<Sh77xxBsc>();
        dispatcher_ = &emu_.Get<PeripheralDispatcher>();
        port_       = dispatcher_->InstallHalfwordPort(kArea4Base, kArea4Last, WordBus());
        bsc_->AddBusWidthListener([this] { Track(); });
        emu_.Get<GuestCpuReset>().RegisterResetReleaseListener([this] { Track(); });
        emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { Track(); });
    }

private:
    bool WordBus() const { return bsc_->AreaBusBytes(kArea4) == kWordBusBytes; }

    void Track() { dispatcher_->SetHalfwordPort(port_, WordBus()); }

    Sh77xxBsc*                           bsc_        = nullptr;
    PeripheralDispatcher*                dispatcher_ = nullptr;
    PeripheralDispatcher::HalfwordPortId port_       = 0u;
};

}

REGISTER_SERVICE(Sh77xxArea4BusPort);

}
