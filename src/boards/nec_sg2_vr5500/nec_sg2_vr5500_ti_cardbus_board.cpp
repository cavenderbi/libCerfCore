#include "../../peripherals/ti_cardbus/ti_cardbus_board.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../socs/vrc5477/vrc5477_intc.h"
#include "nec_sg2_vr5500_family_id.h"

#include <array>
#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint8_t  kPciDevice      = 10u;
constexpr uint32_t kMfunc3Irqser   = 0x00001000u;
constexpr uint8_t  kPinInta        = 1u;
constexpr uint32_t kRoutedThroughM1535 = 0xFFFFFFFFu;
constexpr uint8_t  kGpiLevelsUnknown   = 0x00u;

constexpr std::array<uint32_t, 4> kIntcSourceOfConnectorPin = { 8u, 9u, kRoutedThroughM1535, 11u };

class NecSg2Vr5500TiCardbusBoard : public TiCardbusBoard {
public:
    using TiCardbusBoard::TiCardbusBoard;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecSg2Vr5500;
    }

    uint8_t  PciDevice() const override { return kPciDevice; }
    bool     SerialEepromPresent() const override { return true; }
    uint32_t MultifunctionRoutingAtPowerOn() const override { return kMfunc3Irqser; }
    uint8_t  GeneralPurposeInputLevels() const override { return kGpiLevelsUnknown; }
    void     SetPciInterruptLevel(uint8_t interrupt_pin, bool serialized, bool level) override {
        if (!serialized) {
            if (level)
                emu_.Get<Fatal>().Die("SG2 PCI1225: parallel INT%c terminal asserted; its connection on the "
                                      "add-in card is not modeled", 'A' + interrupt_pin - kPinInta);
            return;
        }
        const uint32_t source = kIntcSourceOfConnectorPin[interrupt_pin - kPinInta];
        if (source == kRoutedThroughM1535) {
            if (level)
                emu_.Get<Fatal>().Die("SG2 PCI1225: card INTC# asserted; the baseboard routes it through "
                                      "the M1535 interrupt router, which is not modeled");
            return;
        }
        auto& intc = emu_.Get<Vrc5477Intc>();
        if (level)
            intc.AssertSource(source);
        else
            intc.DeassertSource(source);
    }
    void     SetIsaIrqLevels(uint8_t function, bool serialized, uint16_t driven, uint16_t irqs) override {
        if (!serialized && driven != 0u)
            emu_.Get<Fatal>().Die("SG2 PCI1225 function %u: parallel ISA IRQ terminals driven (IRQ mask 0x%04X, "
                                  "asserted 0x%04X); their connection on the add-in card is not modeled",
                                  function, driven, irqs);
    }
};

}

REGISTER_SERVICE_AS(NecSg2Vr5500TiCardbusBoard, TiCardbusBoard);

}
