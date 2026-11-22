#include "../../peripherals/open_bus_window.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/philips_pdiusbd12/pdiusbd12_config.h"
#include "hp_jornada_540_family_id.h"
#include "hp_jornada_540_pdiusbd12_ports.h"

namespace cerfcore {

namespace {

class HpJornada540Pdiusbd12OpenBus : public OpenBusWindow {
public:
    using OpenBusWindow::OpenBusWindow;

    uint32_t MmioBase() const override { return kHpJornada540Pdiusbd12DataPa; }
    uint32_t MmioSize() const override { return kHpJornada540Pdiusbd12CommandPa - kHpJornada540Pdiusbd12DataPa + 1u; }

protected:
    std::string_view WindowBoardFamilyId() const override { return BoardFamilyId::HpJornada540; }
    bool WindowOnBoard(const BoardContext&) const override { return emu_.TryGet<Pdiusbd12Config>() == nullptr; }
    const char* WindowTag() const override { return "HpJornada540Pdiusbd12OpenBus"; }
};

}

REGISTER_SERVICE(HpJornada540Pdiusbd12OpenBus);

}
