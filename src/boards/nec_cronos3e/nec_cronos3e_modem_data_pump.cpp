#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/nec_vrc4172/vrc4172_chip_selected.h"
#include "../../peripherals/nec_vrc4172/vrc4172_gpio.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../peripherals/rockwell_rcvdl56dpfl/rcvdl56dpfl_data_pump_peripheral.h"
#include "../../socs/vr41xx/vr41xx_hsp.h"
#include "../../core/board_parts.h"
#include "../../peripherals/rockwell_rcvdl56dpfl/rockwell_rcvdl56dpfl_part_id.h"
#include "nec_cronos3e_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kWindowBase = 0x13100000u;
constexpr uint32_t kWindowSize = 0x40u;
constexpr uint32_t kChipSelect = 3u;
constexpr int      kIrqGpioPin = 1;

class NecCronos3eModemDataPump : public Vrc4172ChipSelected<Rcvdl56dpflDataPumpPeripheral> {
public:
    using Vrc4172ChipSelected<Rcvdl56dpflDataPumpPeripheral>::Vrc4172ChipSelected;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecCronos3e &&
               emu_.Get<BoardParts>().Has(PartId::RockwellRcvdl56dpfl);
    }

    uint32_t MmioBase() const override { return kWindowBase; }
    uint32_t MmioSize() const override { return kWindowSize; }

protected:
    uint32_t ChipSelect() const override { return kChipSelect; }

    void OnChipSelectReady() override {
        gpio_ = &emu_.Get<Vrc4172Gpio>();
        StartDataPump(kRcvdl56dpflTypicalSelfTest);
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<Vr41xxHsp>().RegisterAferstListener([this](bool high) { SetResetPin(high); });
    }

    uint8_t SelectedReadByte(uint32_t addr) override {
        return ReadA1AddressedRegister(addr);
    }

    void SelectedWriteByte(uint32_t addr, uint8_t value) override {
        WriteA1AddressedRegister(addr, value);
    }

    void DriveIrq(bool asserted) override { gpio_->SetPinLevel(kIrqGpioPin, asserted); }

private:
    Vrc4172Gpio* gpio_ = nullptr;
};

}

REGISTER_SERVICE(NecCronos3eModemDataPump);

}
