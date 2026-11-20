#include "../../socs/sh77xx/sh77xx_port_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/pin_net_hold.h"
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "../../socs/sh7707/sh7707_pcc.h"
#include "../sh77xx_port_key_matrix.h"
#include "hp_300_battery.h"
#include "hp_300_family_id.h"
#include "hp_300_member_straps.h"
#include "hp_300_speaker_pin.h"

namespace cerfcore {

namespace {

constexpr uint8_t kPortAKeySense            = 0xFFu;
constexpr uint8_t kPortBKeySense            = 0x07u;
constexpr uint8_t kPortBKeyRows             = 0xF8u;
constexpr uint8_t kPortBWired               = kPortBKeySense | kPortBKeyRows;
constexpr uint8_t kPortCLcdPanelLines       = 0xF1u;
constexpr uint8_t kPortCBacklightControl    = 0x08u;
constexpr uint8_t kPortCSerialPortControls  = 0x06u;
constexpr uint8_t kPortCNetsWithoutADriver  = kPortCLcdPanelLines | kPortCBacklightControl | kPortCSerialPortControls;
constexpr uint8_t kPortDLcdTimingLines      = 0x0Fu;
constexpr uint8_t kPortDExternalPower       = 0x10u;
constexpr uint32_t kPortDPcCardPowerPin     = 5u;
constexpr uint8_t kPortDPcCardPower         = 1u << kPortDPcCardPowerPin;
constexpr uint8_t kPortDChargerStatus       = 0x40u;
constexpr uint8_t kPortDKeyRow              = 0x80u;
constexpr uint8_t kPortDNetsWithoutADriver  = kPortDLcdTimingLines | kPortDPcCardPower | kPortDKeyRow;
constexpr uint8_t kPortDWired               = kPortDNetsWithoutADriver | kPortDExternalPower | kPortDChargerStatus;
constexpr uint32_t kPcCardChannel          = 0u;
constexpr uint32_t kCfChannel              = 1u;
constexpr uint8_t kCardVs1                  = 0x02u;
constexpr uint8_t kCardVs2                  = 0x01u;
constexpr uint8_t kCardVoltageSense         = kCardVs1 | kCardVs2;
constexpr uint8_t kPortHOnKeyColumn         = 0x01u;
constexpr uint8_t kPortHExternalPowerAbsent = 0x02u;
constexpr uint8_t kPortHSerialCarrierOff    = 0x04u;
constexpr uint8_t kPortHPenDown             = 0x08u;
constexpr uint8_t kPortHUndrivenInput       = 0x10u;
constexpr uint8_t kPortHWired =
    kPortHOnKeyColumn | kPortHExternalPowerAbsent | kPortHSerialCarrierOff | kPortHPenDown | kPortHUndrivenInput;
constexpr uint8_t kPortJKeyRow              = 0x10u;
constexpr uint32_t kPortKChargerOffPin     = 4u;
constexpr uint32_t kPortKCfPowerPin        = 5u;
constexpr uint8_t kPortLTouchPlates         = 0x03u;
constexpr uint8_t kPortLPlatformStrap       = 0x10u;
constexpr uint8_t kPortLModelStrap          = 0x20u;
constexpr uint8_t kPortLStraps              = kPortLPlatformStrap | kPortLModelStrap;
constexpr uint8_t kPortLWired               = kPortLTouchPlates | kPortLStraps;
constexpr uint32_t kPortScPcCardPowerPin    = 1u;
constexpr uint8_t kPortScPcCardPower        = 1u << kPortScPcCardPowerPin;
constexpr uint8_t kPortScKeyRow             = 0x08u;
constexpr uint8_t kPortScSerialRtsOff       = 0x40u;
constexpr uint8_t kPortScSerialCtsOff       = 0x80u;
constexpr uint8_t kPortScNetsWithoutADriver =
    kPortScPcCardPower | kPortScKeyRow | kHp300PortScSpeakerOff | kPortScSerialRtsOff;
constexpr uint8_t kPortScWired              = kPortScNetsWithoutADriver | kPortScSerialCtsOff;
constexpr uint8_t kPortScSciReceive         = 0x01u;
constexpr uint8_t kPortScInfraredReceive    = 0x04u;
constexpr uint8_t kPortScSerialReceive      = 0x10u;
constexpr uint8_t kPortScReceiveLines       = kPortScSciReceive | kPortScInfraredReceive | kPortScSerialReceive;
constexpr uint8_t kPortScReceiveLinesAtRest = kPortScSciReceive | kPortScSerialReceive;

class Hp300PortWiring : public Sh77xxPortWiring {
public:
    using Sh77xxPortWiring::Sh77xxPortWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override {
        straps_  = &emu_.Get<Hp300MemberStraps>();
        battery_ = &emu_.Get<Hp300Battery>();
        intc_    = &emu_.Get<Sh77xxIntc>();
        matrix_  = &emu_.Get<Sh77xxPortKeyMatrix>();
        pcc_     = &emu_.Get<Sh7707Pcc>();
    }

    void TrackDrive(const Sh77xxPortData& ports) override {
        nets_.Track(ports);
        const uint8_t port_b = nets_.Levels(kSh77xxPortB);
        for (uint32_t pin = 0; pin < kSh77xxPortPins; ++pin) {
            if (((kPortBKeyRows >> pin) & 1u) == 0u) continue;
            intc_->SetPinLevel(ports.IntcPinOf(kSh77xxPortB, pin), ((port_b >> pin) & 1u) != 0u);
        }
        matrix_->TrackRows(ports);
        battery_->SetChargerEnabled(ports.Drive(kSh77xxPortK, kPortKChargerOffPin) == PinDrive::kLow);
        pcc_->SetSocketPower(kPcCardChannel, ports.Drive(kSh77xxPortD, kPortDPcCardPowerPin) == PinDrive::kLow &&
                                                 ports.Drive(kSh77xxPortSc, kPortScPcCardPowerPin) == PinDrive::kLow);
        pcc_->SetSocketPower(kCfChannel, ports.Drive(kSh77xxPortK, kPortKCfPowerPin) == PinDrive::kLow);
    }

    uint8_t WiredInputs(uint32_t port) const override {
        switch (port) {
            case kSh77xxPortA:  return kPortAKeySense;
            case kSh77xxPortB:  return kPortBWired;
            case kSh77xxPortC:  return kPortCNetsWithoutADriver;
            case kSh77xxPortD:  return kPortDWired;
            case kSh77xxPortF:  return kCardVoltageSense;
            case kSh77xxPortG:  return kCardVoltageSense;
            case kSh77xxPortH:  return kPortHWired;
            case kSh77xxPortJ:  return kPortJKeyRow;
            case kSh77xxPortL:  return kPortLWired;
            case kSh77xxPortSc: return kPortScWired;
            default:            return 0u;
        }
    }

    uint8_t WiredSeparateInputs(uint32_t port) const override {
        return port == kSh77xxPortSc ? kPortScReceiveLines : uint8_t{0u};
    }

    uint8_t SeparateInputLevels(uint32_t port) const override {
        return port == kSh77xxPortSc ? kPortScReceiveLinesAtRest : uint8_t{0u};
    }

    uint8_t NetLevels(uint32_t port) const override {
        switch (port) {
            case kSh77xxPortD:
                return static_cast<uint8_t>((nets_.Levels(port) & kPortDNetsWithoutADriver) | kPortDChargerStatus |
                                            (battery_->OnExternalPower() ? kPortDExternalPower : 0u));
            case kSh77xxPortF: return VoltageSenseLevels(kCfChannel);
            case kSh77xxPortG: return VoltageSenseLevels(kPcCardChannel);
            case kSh77xxPortL:
                return static_cast<uint8_t>((nets_.Levels(port) & kPortLTouchPlates) |
                                            (straps_->Luke() ? kPortLPlatformStrap : 0u) |
                                            (straps_->Model2() ? kPortLModelStrap : 0u));
            default:
                return nets_.Levels(port);
        }
    }

    void SaveState(StateWriter& w) override { nets_.SaveState(w); }

    void RestoreState(StateReader& r) override { nets_.RestoreState(r); }

private:
    uint8_t VoltageSenseLevels(uint32_t channel) const {
        const HitachiPccSocketPins pins = pcc_->Pins(channel);
        return static_cast<uint8_t>((pins.vs1 ? kCardVs1 : 0u) | (pins.vs2 ? kCardVs2 : 0u));
    }

    Hp300MemberStraps*                            straps_  = nullptr;
    Hp300Battery*                                 battery_ = nullptr;
    Sh77xxIntc*                                   intc_    = nullptr;
    Sh77xxPortKeyMatrix*                          matrix_  = nullptr;
    Sh7707Pcc*                                    pcc_     = nullptr;
    PinNetHold<kSh77xxPortCount, kSh77xxPortPins> nets_;
};

}

REGISTER_SERVICE_AS(Hp300PortWiring, Sh77xxPortWiring);

}
