#include "../hp_sh3_serial_port_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "hp_600_family_id.h"
#include "hp_600_input_nets.h"

namespace cerfcore {

namespace {

class Hp600SerialWiring : public HpSh3SerialPortWiring {
public:
    using HpSh3SerialPortWiring::HpSh3SerialPortWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        inputs_ = &emu_.Get<Hp600InputNets>();
        HpSh3SerialPortWiring::OnReady();
    }

    void SetModemInputs(uint32_t, bool cts, bool, bool, bool dcd) override {
        inputs_->Drive(Hp600InputNet::kSerialCtsOff, !cts);
        emu_.Get<Sh77xxIntc>().SetPinLevel(Sh77xxIntcPin::kIrq2, !dcd);
    }

protected:
    ControlPins Pins() const override { return {{kSh77xxPortSc, 0x08u}, {kSh77xxPortH, 0x80u}, {kSh77xxPortSc, 0x40u}}; }

private:
    Hp600InputNets* inputs_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp600SerialWiring, Sh77xxSerialWiring);

}
