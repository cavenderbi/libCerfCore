#include "../../socs/sh77xx/sh77xx_port_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/pin_net_hold.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "hp_600_family_id.h"
#include "hp_600_input_nets.h"
#include "hp_600_battery.h"
#include "hp_600_led.h"
#include "../sh77xx_port_key_matrix.h"
#include "hp_600_notification_led_net.h"

namespace cerfcore {

namespace {

constexpr uint8_t kPortCKeySenseAndLid = 0xFFu;
constexpr uint8_t kPortDRowsAndPins    = 0xFBu;
constexpr uint8_t kPortERowsAndPins    = 0xFFu;
constexpr uint8_t kPortFKeySense       = 0xFFu;
constexpr uint8_t kPortGInputs         = 0xFFu;
constexpr uint8_t kPortHOnKey          = 0x01u;
constexpr uint8_t kPortHIrq1Line       = 0x02u;
constexpr uint8_t kPortHSerialCarrier  = 0x04u;
constexpr uint8_t kPortHPenDown        = 0x08u;
constexpr uint8_t kPortHCompanionIrq   = 0x10u;
constexpr uint8_t kPortHUnreadInput    = 0x40u;
constexpr uint8_t kPortHWired = kPortHOnKey | kPortHIrq1Line | kPortHSerialCarrier | kPortHPenDown |
                                kPortHCompanionIrq | kPortHUnreadInput;
constexpr uint8_t kPortLTouchPlates    = 0x06u;
constexpr uint8_t kPortLCharging       = 0x20u;
constexpr uint8_t kPortLWired          = kPortLTouchPlates | kPortLCharging;
constexpr uint8_t kPortScUnreadInputs  = 0x22u;

constexpr uint8_t kPortGLidOpen        = 0x01u;
constexpr uint8_t kPortGMainBatteryOut = 0x04u;
constexpr uint8_t kPortGBoardDriven    = kPortGLidOpen | kPortGMainBatteryOut;

class Hp600PortWiring : public Sh77xxPortWiring {
public:
    using Sh77xxPortWiring::Sh77xxPortWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        matrix_  = &emu_.Get<Sh77xxPortKeyMatrix>();
        battery_ = &emu_.Get<Hp600Battery>();
        inputs_  = &emu_.Get<Hp600InputNets>();
        led_     = &emu_.Get<Hp600Led>();
    }

    void TrackDrive(const Sh77xxPortData& ports) override {
        nets_.Track(ports);
        matrix_->TrackRows(ports);
        led_->TrackDrive((nets_.Levels(kSh77xxPortJ) & kHp600PortJNotificationLed) != 0u);
    }

    uint8_t WiredInputs(uint32_t port) const override {
        return static_cast<uint8_t>(FamilyInputs(port) | inputs_->WiredInputs(port));
    }

    uint8_t NetLevels(uint32_t port) const override {
        uint8_t nets = nets_.Levels(port);
        if (port == kSh77xxPortL) {
            nets = static_cast<uint8_t>(battery_->Charging() ? nets & ~kPortLCharging : nets | kPortLCharging);
        }
        if (port == kSh77xxPortG) nets = static_cast<uint8_t>((nets & ~kPortGBoardDriven) | kPortGLidOpen);
        return inputs_->NetLevels(port, nets);
    }

    void SaveState(StateWriter& w) override { nets_.SaveState(w); }

    void RestoreState(StateReader& r) override { nets_.RestoreState(r); }

private:
    static uint8_t FamilyInputs(uint32_t port) {
        switch (port) {
            case kSh77xxPortC:  return kPortCKeySenseAndLid;
            case kSh77xxPortD:  return kPortDRowsAndPins;
            case kSh77xxPortE:  return kPortERowsAndPins;
            case kSh77xxPortF:  return kPortFKeySense;
            case kSh77xxPortG:  return kPortGInputs;
            case kSh77xxPortH:  return kPortHWired;
            case kSh77xxPortL:  return kPortLWired;
            case kSh77xxPortSc: return kPortScUnreadInputs;
            default:            return 0u;
        }
    }

    Sh77xxPortKeyMatrix*                          matrix_  = nullptr;
    Hp600Battery*                                 battery_ = nullptr;
    Hp600InputNets*                               inputs_  = nullptr;
    Hp600Led*                                     led_     = nullptr;
    PinNetHold<kSh77xxPortCount, kSh77xxPortPins> nets_;
};

}

REGISTER_SERVICE_AS(Hp600PortWiring, Sh77xxPortWiring);

}
