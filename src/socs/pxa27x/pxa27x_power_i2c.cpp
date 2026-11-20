#include "pxa27x_power_i2c.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "pxa270_id.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../irq_controller.h"

namespace cerfcore {

REGISTER_SERVICE(Pxa27xPowerI2c);

namespace {

constexpr int kIntcPowerI2c = 6;

}

bool Pxa27xPowerI2c::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetSocId() == SocId::Pxa270;
}

void Pxa27xPowerI2c::OnReady() {
    emu_.Get<PeripheralDispatcher>().Register(this);
}

uint32_t Pxa27xPowerI2c::ReadWord(uint32_t addr) {
    uint32_t value = 0;
    if (!unit_.Read(addr - MmioBase(), value)) HaltUnsupportedAccess("ReadWord", addr, 0);
    return value;
}

void Pxa27xPowerI2c::WriteWord(uint32_t addr, uint32_t value) {
    const uint32_t off = addr - MmioBase();
    if (off == Pxa27xI2cUnit::kICR && (value & IntelI2cUnit::kIcrTransferByte) != 0u)
        HaltUnsupportedAccess("power I2C transfer to the external voltage regulator", addr, value);
    if (!unit_.Write(off, value, nullptr)) HaltUnsupportedAccess("WriteWord", addr, value);
    UpdateIrq();
}

void Pxa27xPowerI2c::ResetRegisters() {
    unit_.Reset(nullptr);
    UpdateIrq();
}

void Pxa27xPowerI2c::SequencerTransmitted(uint8_t last_byte, bool stop) {
    unit_.SequencerTransmitted(last_byte, stop);
    UpdateIrq();
}

void Pxa27xPowerI2c::UpdateIrq() {
    auto& intc = emu_.Get<IrqController>();
    if (unit_.InterruptActive()) intc.AssertIrq  (kIntcPowerI2c);
    else                         intc.DeAssertIrq(kIntcPowerI2c);
}

void Pxa27xPowerI2c::SaveState(StateWriter& w) { unit_.SaveState(w); }

void Pxa27xPowerI2c::RestoreState(StateReader& r) { unit_.RestoreState(r); }

}
