#include "../hp_sh3_serial_port_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

class Hp300SerialWiring : public HpSh3SerialPortWiring {
public:
    using HpSh3SerialPortWiring::HpSh3SerialPortWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override {
        intc_ = &emu_.Get<Sh77xxIntc>();
        HpSh3SerialPortWiring::OnReady();
    }

    void SetModemInputs(uint32_t, bool cts, bool, bool, bool dcd) override {
        intc_->SetPinLevel(Sh77xxIntcPin::kIrq5, !cts);
        intc_->SetPinLevel(Sh77xxIntcPin::kIrq2, !dcd);
    }

protected:
    ControlPins Pins() const override { return {{kSh77xxPortC, 0x02u}, {kSh77xxPortJ, 0x02u}, {kSh77xxPortSc, 0x40u}}; }

private:
    Sh77xxIntc* intc_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp300SerialWiring, Sh77xxSerialWiring);

}
