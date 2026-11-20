#include "../../socs/vr41xx/vr41xx_piu_panel.h"
#include "../../socs/vr41xx/vr41xx_piu_regs.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/boards/board_context.h>
#include "casio_tomcat_family_id.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

namespace {

using cerf_vr41xx_piu_detail::kUndrivenPortSample;

class CasioTomcatTouchPanel : public Vr41xxPiuPanel {
public:
    using Vr41xxPiuPanel::Vr41xxPiuPanel;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioTomcat;
    }

    /* casio_cassiopeia_e55 touch.dll sub_14D05DC writes PIUCNTREG 14, 0x100, 0x200 and 4; none
       carries D5 PADSCANTYPE, which VR4111 UM 20.3.1 p426 defines as "0: Prohibit". */
    std::optional<uint16_t> PressureSample(bool) override { return std::nullopt; }

    uint16_t ReleasedPlateSample() override { return kUndrivenPortSample; }

    std::optional<uint16_t> PortSample(uint16_t) override { return kUndrivenPortSample; }
};

}  /* namespace */

REGISTER_SERVICE_AS_ALSO(CasioTomcatTouchPanel, Vr41xxPiuScreen, Vr41xxPiuPanel);

}
