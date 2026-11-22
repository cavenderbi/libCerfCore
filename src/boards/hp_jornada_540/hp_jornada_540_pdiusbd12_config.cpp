#include "../../peripherals/philips_pdiusbd12/pdiusbd12_config.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "hp_jornada_540_family_id.h"
#include "hp_jornada_545_id.h"
#include "hp_jornada_547_id.h"
#include "hp_jornada_548_id.h"
#include "hp_jornada_540_pdiusbd12_ports.h"

namespace cerfcore {

namespace {

constexpr uint32_t      kInterruptPint = 1u;
constexpr Sh77xxIntcPin kInterruptPin  = Sh77xxPintPin(kInterruptPint);

class HpJornada540Pdiusbd12Config : public Pdiusbd12Config {
public:
    using Pdiusbd12Config::Pdiusbd12Config;

    bool ShouldRegister() override {
        auto& board = emu_.Get<BoardContext>();
        if (board.GetBoardFamilyId() != BoardFamilyId::HpJornada540) return false;
        const auto id = board.GetBoardId();
        return id == BoardId::HpJornada545 || id == BoardId::HpJornada547 || id == BoardId::HpJornada548;
    }

    void OnReady() override { intc_ = &emu_.Get<Sh77xxIntc>(); }

    uint32_t DataPortPa() const override { return kHpJornada540Pdiusbd12DataPa; }
    uint32_t CommandPortPa() const override { return kHpJornada540Pdiusbd12CommandPa; }
    void     DriveInterrupt(bool asserted) override { intc_->SetPinLevel(kInterruptPin, !asserted); }
    bool     InterruptWakesStandby() override { return intc_->PintWakesStandby(kInterruptPint, false); }

private:
    Sh77xxIntc* intc_ = nullptr;
};

}

REGISTER_SERVICE_AS(HpJornada540Pdiusbd12Config, Pdiusbd12Config);

}
