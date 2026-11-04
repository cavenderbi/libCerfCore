#include "nec_vrc4171a.h"
#include "../pcic82365/isa_exca_mem_window.h"

#include <cstdint>

namespace cerfcore {

namespace {

class NecVrc4171aMemWindow : public IsaExcaMemWindow<NecVrc4171a, NecVrc4171aBoard, 0x04000000u> {
public:
    using IsaExcaMemWindow::IsaExcaMemWindow;
};

}

REGISTER_SERVICE(NecVrc4171aMemWindow);

}
