#include "nec_vrc4171a.h"
#include "../pcic82365/isa_exca_mem_window.h"

#include <cstdint>

namespace cerfcore {

namespace {

class NecVrc4171aMemWindow : public IsaExcaMemWindow<NecVrc4171a, 0x04000000u> {
public:
    using IsaExcaMemWindow::IsaExcaMemWindow;

protected:
    uint32_t AreaBase() override { return emu_.Get<NecVrc4171aBoard>().IsaMemBase(); }
};

}

REGISTER_SERVICE(NecVrc4171aMemWindow);

}
