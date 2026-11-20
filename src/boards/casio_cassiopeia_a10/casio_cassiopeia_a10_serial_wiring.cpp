#include "../sh77xx_serial_lines_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/hibernation.h>
#include "../../socs/hd6417096/hd6417096_scif_channel.h"
#include "../../socs/sh77xx/sh77xx_sci_channel.h"
#include "casio_cassiopeia_a10_connector_route.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_power_control.h"
#include "casio_cassiopeia_a10_rts_line.h"
#include "casio_cassiopeia_a10_serial_connector.h"
#include "casio_cassiopeia_a10_serial_control.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

namespace {

constexpr uint16_t kPowerControlSupply = 0x0021u;
constexpr uint16_t kSerialControlSupply = 0x000Cu;

class CasioCassiopeiaA10SerialWiring : public Sh77xxSerialLinesWiring {
public:
    using Sh77xxSerialLinesWiring::Sh77xxSerialLinesWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        power_     = &emu_.Get<CasioCassiopeiaA10PowerControl>();
        control_   = &emu_.Get<CasioCassiopeiaA10SerialControl>();
        connector_ = &emu_.Get<CasioCassiopeiaA10SerialConnector>();
        route_     = &emu_.Get<CasioCassiopeiaA10ConnectorRoute>();
        rts_       = &emu_.Get<CasioCassiopeiaA10RtsLine>();
        power_->AddChangeListener([this] { TrackLines(); });
        connector_->AddChangeListener([this] { TrackLines(); });
        route_->AddChangeListener([this] { TrackLines(); });
        rts_->AddChangeListener([this] { TrackLines(); });
        emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { TrackLines(); });
        TrackLines();
    }

    std::optional<ObjectIdentity> Cradle(uint32_t) const override { return std::nullopt; }

    MachineObject* ShareLine(uint32_t channel_base, SerialLine& line) override {
        if (channel_base == kSh77xxSciBase) return connector_->AttachLine(CasioCassiopeiaA10SerialUnit::kSci, line);
        if (channel_base == kHd6417096ScifBase) {
            return connector_->AttachLine(CasioCassiopeiaA10SerialUnit::kScif, line);
        }
        return nullptr;
    }

    void SetModemInputs(uint32_t, bool, bool, bool, bool) override {}

private:
    static uint32_t ChannelBase(std::optional<CasioCassiopeiaA10SerialUnit> unit) {
        if (unit == CasioCassiopeiaA10SerialUnit::kSci) return kSh77xxSciBase;
        if (unit == CasioCassiopeiaA10SerialUnit::kScif) return kHd6417096ScifBase;
        return kNoChannel;
    }

    void TrackLines() {
        const std::optional<CasioCassiopeiaA10SerialUnit> unit   = route_->Routed();
        const std::optional<CasioCassiopeiaA10SerialUnit> joined = route_->Joined();
        connector_->Route(unit, joined);
        const uint32_t base    = ChannelBase(unit);
        const bool     powered = base != kNoChannel && ((power_->Value() & kPowerControlSupply) == kPowerControlSupply ||
                                                         (control_->Value() & kSerialControlSupply) == kSerialControlSupply);
        const bool     dtr     = (connector_->Value() & kCasioCassiopeiaA10LineDtr) != 0u;
        SetLines(base, powered, powered && dtr, powered && rts_->Asserted(), ChannelBase(joined));
    }

    CasioCassiopeiaA10PowerControl*    power_     = nullptr;
    CasioCassiopeiaA10SerialControl*   control_   = nullptr;
    CasioCassiopeiaA10SerialConnector* connector_ = nullptr;
    CasioCassiopeiaA10ConnectorRoute*  route_     = nullptr;
    CasioCassiopeiaA10RtsLine*         rts_       = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10SerialWiring, Sh77xxSerialWiring);

}
