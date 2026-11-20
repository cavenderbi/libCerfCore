#include "../sh77xx_scif2_serial_port_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "../../socs/sh77xx/sh77xx_scif.h"
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

constexpr uint8_t kPortCDriverPowerOff = 0x02u;
constexpr uint8_t kPortJDtrOff         = 0x02u;
constexpr uint8_t kPortScRtsOff        = 0x40u;

class Hp300SerialWiring : public Sh77xxScif2SerialPortWiring {
public:
    using Sh77xxScif2SerialPortWiring::Sh77xxScif2SerialPortWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override {
        ports_ = &emu_.Get<Sh77xxPortWiring>();
        intc_  = &emu_.Get<Sh77xxIntc>();
        emu_.Get<Sh77xxPortData>().AddDriveListener([this] { TrackNets(); });
        TrackNets();
    }

    void SetModemInputs(uint32_t, bool cts, bool, bool, bool dcd) override {
        intc_->SetPinLevel(Sh77xxIntcPin::kIrq5, !cts);
        intc_->SetPinLevel(Sh77xxIntcPin::kIrq2, !dcd);
    }

private:
    void TrackNets() {
        const bool powered = (ports_->NetLevels(kSh77xxPortC) & kPortCDriverPowerOff) == 0u;
        SetLines(kSh77xxScif2Base, powered, powered && (ports_->NetLevels(kSh77xxPortJ) & kPortJDtrOff) == 0u,
                 powered && (ports_->NetLevels(kSh77xxPortSc) & kPortScRtsOff) == 0u);
    }

    Sh77xxPortWiring* ports_ = nullptr;
    Sh77xxIntc*       intc_  = nullptr;
};

}

REGISTER_SERVICE_AS(Hp300SerialWiring, Sh77xxSerialWiring);

}
