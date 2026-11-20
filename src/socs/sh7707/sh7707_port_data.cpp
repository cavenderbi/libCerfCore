#include "../sh77xx/sh77xx_area0_bus_width.h"
#include "../sh77xx/sh77xx_bsc.h"
#include "../sh77xx/sh77xx_port_data_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

using sh77xx_port_data::kIntcInInputModes;
using sh77xx_port_data::kIntcInOtherAndInputModes;
using sh77xx_port_data::kIntcInOtherMode;

constexpr sh77xx_port_data::Model kSh7707PortData{{
    {0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xFFu, Sh77xxIntcPin::kPint0, 0x00u, 0x00u, kIntcInInputModes},
    {0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xFFu, Sh77xxIntcPin::kPint8, 0x00u, 0x00u, kIntcInInputModes},
    {0x00u, 0x00u, 0x00u, 0xFFu},
    {0x50u, 0x00u, 0x00u, 0xAFu},
    {0x00u, 0x00u, 0x00u, 0xFFu},
    {0xFFu, 0x00u, 0x00u, 0x00u},
    {0xFFu, 0x00u, 0x00u, 0x00u},
    {0x7Fu, 0x00u, 0x00u, 0x80u, 0x00u, 0x1Fu, Sh77xxIntcPin::kIrq0, 0x00u, 0x00u, kIntcInOtherAndInputModes,
     kIntcInOtherMode},
    {0x00u, 0x00u, 0x00u, 0xFFu},
    {0x00u, 0x00u, 0x00u, 0xDFu},
    {0xFFu, 0x00u, 0x00u, 0xC0u, 0x00u, 0x00u, Sh77xxIntcPin::kCount, 0xFFu},
    {0x80u, 0x2Au, 0x00u, 0x40u, 0x15u, 0x80u, Sh77xxIntcPin::kIrq5, 0x00u, 0x15u, kIntcInOtherAndInputModes, 0x00u,
     0x00u, Sh77xxIntcPin::kCount, 0x15u},
}};

using Sh7707PortDataModel = sh77xx_port_data::Sh77xxPortDataBase<SocId::Sh7707, kSh7707PortData>;

class Sh7707PortData : public Sh7707PortDataModel {
public:
    using Sh7707PortDataModel::Sh7707PortDataModel;

    void OnReady() override {
        bsc_       = &emu_.Get<Sh77xxBsc>();
        area0_bus_ = &emu_.Get<Sh77xxArea0BusWidth>();
        Sh7707PortDataModel::OnReady();
        bsc_->AddBusWidthListener([this] { TrackDrive(); });
    }

    PinDrive Drive(uint32_t port, uint32_t pin) const override {
        if (OnUpperDataBus(port) && !UpperDataBusIsPortPins()) return PinDrive::kFunction;
        return Sh7707PortDataModel::Drive(port, pin);
    }

    uint8_t ReadByte(uint32_t addr) override {
        if (OnUpperDataBus(Port(addr, "ReadByte", 0u)) && !UpperDataBusIsPortPins()) {
            HaltUnsupportedAccess("ReadByte with an area bus that is not 8 or 16 bits wide", addr, 0u);
        }
        return Sh7707PortDataModel::ReadByte(addr);
    }

private:
    static bool OnUpperDataBus(uint32_t port) { return port < kSh77xxPortC; }

    static bool ByteOrWord(uint32_t bus_bytes) { return bus_bytes == 1u || bus_bytes == 2u; }

    bool UpperDataBusIsPortPins() const {
        if (!ByteOrWord(area0_bus_->Bytes())) return false;
        for (uint32_t area = kSh77xxBcr2FirstArea; area <= kSh77xxBcr2LastArea; ++area) {
            if (!ByteOrWord(bsc_->AreaBusBytes(area))) return false;
        }
        return true;
    }

    Sh77xxBsc*                 bsc_       = nullptr;
    const Sh77xxArea0BusWidth* area0_bus_ = nullptr;
};

}

REGISTER_SERVICE_AS(Sh7707PortData, Sh77xxPortData);

}
