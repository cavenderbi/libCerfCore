#include "pr31x00_card_space.h"

#include <cerfcore/boards/board_context.h>
#include "pr31500_id.h"
#include "pr31700_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/peripheral_dispatcher.h"

#include <cstdint>

namespace cerfcore {

namespace {

/* Card 1 and Card 2 I/O-or-Attribute space, 64 MB each, adjacent (Table 4.2.1). */
constexpr uint32_t kBase = 0x08000000u;
constexpr uint32_t kSize = 0x08000000u;

class Pr31x00CardCtrlWindow : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        const std::string_view soc = emu_.Get<BoardContext>().GetSocId();
        return soc == SocId::Pr31500 || soc == SocId::Pr31700;
    }

    void OnReady() override { emu_.Get<PeripheralDispatcher>().Register(this); }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    uint8_t ReadByte(uint32_t addr) override {
        return emu_.Get<Pr31x00CardSpace>().ReadCtrl8(addr - kBase);
    }
    uint16_t ReadHalf(uint32_t addr) override {
        return emu_.Get<Pr31x00CardSpace>().ReadCtrl16(addr - kBase);
    }
    void WriteByte(uint32_t addr, uint8_t value) override {
        emu_.Get<Pr31x00CardSpace>().WriteCtrl8(addr - kBase, value);
    }
    void WriteHalf(uint32_t addr, uint16_t value) override {
        emu_.Get<Pr31x00CardSpace>().WriteCtrl16(addr - kBase, value);
    }
};

}  /* namespace */

REGISTER_SERVICE(Pr31x00CardCtrlWindow);

}
