#include "../../peripherals/intel_82365sl/intel_82365sl.h"
#include "../../peripherals/pcic82365/isa_exca_mem_window.h"
#include "../../socs/vr4101/vr4101_expansion_bus.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr Vr4101DeviceMode kMode = Vr4101DeviceMode::kSixteenBit;

class NecMobilepro200IsaMemWindow16
    : public IsaExcaMemWindow<Intel82365sl, kVr4101ExpansionAreaBytes, Vr4101ExpansionHost<kMode>> {
public:
    using IsaExcaMemWindow::IsaExcaMemWindow;

protected:
    uint32_t AreaBase() override { return Vr4101ExpansionMemBase(kMode); }
};

}

REGISTER_SERVICE(NecMobilepro200IsaMemWindow16);

}
