#include "../../socs/sh7707/sh7707_lcd_panel.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../peripherals/pin_drive.h"
#include "../../socs/sh77xx/sh77xx_dac.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kDisplayOffPin        = 0u;
constexpr uint32_t kPanelSupplyOffPin    = 5u;
constexpr uint32_t kPanelDriveDacChannel = 0u;

class Hp300LcdPanel : public Sh7707LcdPanel {
public:
    using Sh7707LcdPanel::Sh7707LcdPanel;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override {
        dac_   = &emu_.Get<Sh77xxDac>();
        ports_ = &emu_.Get<Sh77xxPortData>();
        dac_->AddOutputListener([this] { NotifyInputsChanged(); });
        ports_->AddDriveListener([this] { NotifyInputsChanged(); });
    }

    bool ShowsColor() const override { return false; }
    bool TakesLowerPanelData() const override { return false; }

    bool Lit() const override {
        return NetLow(kSh77xxPortD, kDisplayOffPin) && NetLow(kSh77xxPortJ, kPanelSupplyOffPin) &&
               dac_->ChannelOutput(kPanelDriveDacChannel).enabled;
    }

private:
    bool NetLow(uint32_t port, uint32_t pin) const {
        const PinDrive drive = ports_->Drive(port, pin);
        if (drive == PinDrive::kUndriven) {
            emu_.Get<Fatal>().Die("Hp300LcdPanel: panel control net on port %u pin %u is undriven", port, pin);
        }
        return drive == PinDrive::kLow;
    }

    Sh77xxDac*      dac_   = nullptr;
    Sh77xxPortData* ports_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp300LcdPanel, Sh7707LcdPanel);

}
