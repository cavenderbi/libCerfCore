#include "ricoh_rf5c396.h"
#include "../pcic82365/isa_exca_io_window.h"

#include <cstdint>

namespace cerfcore {

namespace {

class RicohRf5c396IoWindow : public IsaExcaIoWindow<RicohRf5c396, 0x00010000u> {
public:
    using IsaExcaIoWindow::IsaExcaIoWindow;

protected:
    uint32_t AreaBase() override { return emu_.Get<RicohRf5c396Board>().IsaIoBase(); }
};

}

REGISTER_SERVICE(RicohRf5c396IoWindow);

}
