#include "../sh77xx_scif2_serial_port_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "../../socs/sh77xx/sh77xx_scif.h"
#include "hp_600_family_id.h"
#include "hp_600_input_nets.h"

namespace cerfcore {

namespace {

constexpr uint8_t kPortScDriverPowerOff = 0x08u;
constexpr uint8_t kPortScRtsOff         = 0x40u;
constexpr uint8_t kPortHDtrOff          = 0x80u;

class Hp600SerialWiring : public Sh77xxScif2SerialPortWiring {
public:
    using Sh77xxScif2SerialPortWiring::Sh77xxScif2SerialPortWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        ports_  = &emu_.Get<Sh77xxPortWiring>();
        inputs_ = &emu_.Get<Hp600InputNets>();
        emu_.Get<Sh77xxPortData>().AddDriveListener([this] { TrackNets(); });
        TrackNets();
    }

    void SetModemInputs(uint32_t, bool cts, bool, bool, bool dcd) override {
        inputs_->Drive(Hp600InputNet::kSerialCtsOff, !cts);
        emu_.Get<Sh77xxIntc>().SetPinLevel(Sh77xxIntcPin::kIrq2, !dcd);
    }

private:
    void TrackNets() {
        const uint8_t port_sc = ports_->NetLevels(kSh77xxPortSc);
        const uint8_t port_h  = ports_->NetLevels(kSh77xxPortH);
        const bool    powered = (port_sc & kPortScDriverPowerOff) == 0u;
        SetLines(kSh77xxScif2Base, powered, powered && (port_h & kPortHDtrOff) == 0u,
                 powered && (port_sc & kPortScRtsOff) == 0u);
    }

    Sh77xxPortWiring* ports_  = nullptr;
    Hp600InputNets*   inputs_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp600SerialWiring, Sh77xxSerialWiring);

}
