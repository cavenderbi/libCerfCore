#include "ricoh_rf5c396.h"
#include "../pcic82365/isa_exca_mem_window.h"

#include <cstdint>

namespace cerfcore {

namespace {

class RicohRf5c396MemWindow : public IsaExcaMemWindow<RicohRf5c396, RicohRf5c396Board, 0x01000000u> {
public:
    using IsaExcaMemWindow::IsaExcaMemWindow;
};

}

REGISTER_SERVICE(RicohRf5c396MemWindow);

}
