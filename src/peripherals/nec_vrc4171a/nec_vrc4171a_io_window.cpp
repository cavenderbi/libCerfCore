#include "nec_vrc4171a.h"
#include "../pcic82365/isa_exca_io_window.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kSize        = 0x01010000u;
constexpr uint32_t kBlockShift  = 16u;
constexpr uint32_t kPortMask    = 0xFFFFu;
constexpr uint32_t kCardIoBlock = 0x100u;

class NecVrc4171aIoWindow : public IsaExcaIoWindow<NecVrc4171a, kSize> {
public:
    using IsaExcaIoWindow::IsaExcaIoWindow;

protected:
    uint32_t AreaBase() override { return emu_.Get<NecVrc4171aBoard>().IsaIoBase(); }
    uint32_t PortOf(uint32_t off) const override { return off & kPortMask; }
    bool     ControllerDecodes(uint32_t off) override { return !Pcic().IoUpperDecode() || CardIoSpace(off); }
    bool     CardIoSpace(uint32_t off) const override { return (off >> kBlockShift) == kCardIoBlock; }
    bool     IndexDataHalfword() const override { return false; }
    bool     ChipHalfwordPort(uint32_t port) const override { return NecVrc4171a::IsTopLevelPort(port); }
    uint16_t ReadChipHalfwordPort(uint32_t port) override { return Pcic().ReadTopLevel(port); }
    void     WriteChipHalfwordPort(uint32_t port, uint16_t value) override { Pcic().WriteTopLevel(port, value); }
};

}

REGISTER_SERVICE(NecVrc4171aIoWindow);

}
