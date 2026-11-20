#include "../../socs/sh77xx/sh77xx_port_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/pin_net_hold.h"
#include "../../socs/hd6417096/hd6417096_port.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "casio_cassiopeia_a10_audio_mute_pin.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_serial_connector.h"
#include "casio_cassiopeia_a10_vdet1r_line.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint8_t kCtsPin     = 0x20u;
constexpr uint8_t kHeldInputs = 0x88u;
constexpr uint8_t kNetInputs  = kHeldInputs | kCasioCassiopeiaA10AudioMutePin;

class CasioCassiopeiaA10PortWiring : public Sh77xxPortWiring {
public:
    using Sh77xxPortWiring::Sh77xxPortWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        vdet1r_    = &emu_.Get<CasioCassiopeiaA10Vdet1rLine>();
        connector_ = &emu_.Get<CasioCassiopeiaA10SerialConnector>();
    }

    void TrackDrive(const Sh77xxPortData& ports) override { nets_.Track(ports); }

    uint8_t WiredInputs(uint32_t) const override {
        return static_cast<uint8_t>(vdet1r_->PortPins() | kCtsPin | kNetInputs);
    }

    uint8_t NetLevels(uint32_t port) const override {
        const uint8_t driven = static_cast<uint8_t>(vdet1r_->PortPins() | kCtsPin);
        const uint8_t cts    = connector_->Cts() ? 0u : kCtsPin;
        return static_cast<uint8_t>((nets_.Levels(port) & ~driven) | vdet1r_->PortLevels() | cts);
    }

    void SaveState(StateWriter& w) override { nets_.SaveState(w); }
    void RestoreState(StateReader& r) override { nets_.RestoreState(r); }

private:
    CasioCassiopeiaA10Vdet1rLine*                        vdet1r_    = nullptr;
    CasioCassiopeiaA10SerialConnector*                   connector_ = nullptr;
    PinNetHold<kHd6417096PortCount, kHd6417096PortPins> nets_{{kCasioCassiopeiaA10AudioMutePin}};
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10PortWiring, Sh77xxPortWiring);

}
