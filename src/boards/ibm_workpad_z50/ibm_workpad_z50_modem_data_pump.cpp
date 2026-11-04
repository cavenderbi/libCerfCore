#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../peripherals/rockwell_rcvdl56dpfl/rcvdl56dpfl_data_pump_peripheral.h"
#include "../../peripherals/rockwell_rcvdl56dpfl/rcvdl56dpfl_self_test.h"
#include "../../socs/vr41xx/vr41xx_hsp.h"
#include "ibm_workpad_z50_family_id.h"
#include "ibm_workpad_z50_member_straps.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kWindowBase = 0x161C0000u;
constexpr uint32_t kWindowSize = 0x40u;

constexpr Rcvdl56dpflSelfTest kSelfTest = [] {
    Rcvdl56dpflSelfTest t = kRcvdl56dpflTypicalSelfTest;
    t.ram1_checksum = 0x0451u;
    t.ram2_checksum = 0x49A5u;
    return t;
}();

class IbmWorkpadZ50ModemDataPump : public Rcvdl56dpflDataPumpPeripheral {
public:
    using Rcvdl56dpflDataPumpPeripheral::Rcvdl56dpflDataPumpPeripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50 &&
               emu_.Get<IbmWorkpadZ50MemberStraps>().ModemFitted();
    }

    void OnReady() override {
        StartDataPump(kSelfTest);
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<Vr41xxHsp>().RegisterAferstListener([this](bool high) { SetResetPin(high); });
    }

    uint32_t MmioBase() const override { return kWindowBase; }
    uint32_t MmioSize() const override { return kWindowSize; }

    uint8_t ReadByte(uint32_t addr) override { return ReadA1AddressedRegister(addr); }
    void    WriteByte(uint32_t addr, uint8_t value) override { WriteA1AddressedRegister(addr, value); }

protected:
    void DriveIrq(bool) override {}
};

}

REGISTER_SERVICE(IbmWorkpadZ50ModemDataPump);

}
