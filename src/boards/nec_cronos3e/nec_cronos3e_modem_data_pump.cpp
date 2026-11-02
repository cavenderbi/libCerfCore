#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/nec_vrc4172/vrc4172_chip_selected.h"
#include "../../peripherals/nec_vrc4172/vrc4172_gpio.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../peripherals/rockwell_rcvdl56dpfl/rcvdl56dpfl_data_pump_peripheral.h"
#include "../../socs/vr41xx/vr41xx_hsp.h"
#include "nec_cronos3e_family_id.h"
#include "nec_mobilegear2_r430_id.h"

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
        const auto& bd = emu_.Get<BoardContext>();
        return bd.GetBoardFamilyId() == BoardFamilyId::NecCronos3e &&
               bd.GetBoardId() != BoardId::NecMobilegear2R430;
    }

    uint32_t MmioBase() const override { return kWindowBase; }
    uint32_t MmioSize() const override { return kWindowSize; }

protected:
    uint32_t ChipSelect() const override { return kChipSelect; }

    void OnChipSelectReady() override {
        gpio_ = &emu_.Get<Vrc4172Gpio>();
        StartDataPump();
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<Vr41xxHsp>().RegisterAferstListener([this](bool high) { SetResetPin(high); });
    }

    uint8_t SelectedReadByte(uint32_t addr) override {
        return ReadPumpRegister(RegisterOf(addr, 0u));
    }

    void SelectedWriteByte(uint32_t addr, uint8_t value) override {
        WritePumpRegister(RegisterOf(addr, value), value);
    }

    void DriveIrq(bool asserted) override { gpio_->SetPinLevel(kIrqGpioPin, asserted); }

private:
    uint32_t RegisterOf(uint32_t addr, uint8_t value) {
        const uint32_t offset = addr - kWindowBase;
        if (offset & 1u) HaltUnsupportedAccess("odd-address byte access", addr, value);
        return offset >> 1;
    }

    Vrc4172Gpio* gpio_ = nullptr;
};

}

REGISTER_SERVICE(NecCronos3eModemDataPump);

}
