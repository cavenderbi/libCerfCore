#include "../../peripherals/intel_82365sl/intel_82365sl.h"
#include "../../peripherals/pcic82365/isa_exca_io_window.h"
#include "../../socs/vr4101/vr4101_expansion_bus.h"
#include "nec_mobilepro_200_isa_ports.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr Vr4101DeviceMode kMode = Vr4101DeviceMode::kSixteenBit;

class NecMobilepro200IsaIoWindow16
    : public IsaExcaIoWindow<Intel82365sl, kNecMobilepro200IsaDebugBlockPort, Vr4101ExpansionHost<kMode>> {
public:
    using IsaExcaIoWindow::IsaExcaIoWindow;

protected:
    uint32_t AreaBase() override { return Vr4101ExpansionIoBase(kMode); }
    bool     CarriesControllerState() const override { return false; }
};

}

REGISTER_SERVICE(NecMobilepro200IsaIoWindow16);

}
