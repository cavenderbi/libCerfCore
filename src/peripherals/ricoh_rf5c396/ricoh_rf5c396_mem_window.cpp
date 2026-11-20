#include "ricoh_rf5c396.h"
#include "../pcic82365/isa_exca_mem_window.h"

#include <cstdint>

namespace cerfcore {

namespace {

class RicohRf5c396MemWindow : public IsaExcaMemWindow<RicohRf5c396, 0x01000000u> {
public:
    using IsaExcaMemWindow::IsaExcaMemWindow;

protected:
    uint32_t AreaBase() override { return emu_.Get<RicohRf5c396Board>().IsaMemBase(); }
};

}

REGISTER_SERVICE(RicohRf5c396MemWindow);

}
