#include "../../socs/sh77xx/sh77xx_port_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/pin_drive.h"
#include "../../peripherals/pin_net_hold.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "hp_jornada_540_audio_request_line.h"
#include "hp_jornada_540_buttons.h"
#include "hp_jornada_540_eeprom_bus.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

constexpr uint8_t  kPortCWiredInputs   = 0xEFu;
constexpr uint8_t  kPortDUnreadInputs  = 0x43u;
constexpr uint8_t  kPortDAudioRequest  = 0x10u;
constexpr uint8_t  kPortDWiredInputs   = kPortDUnreadInputs | kPortDAudioRequest;
constexpr uint32_t kAudioRequestDreq   = 0u;
constexpr uint8_t kPortJEepromScl    = 0x08u;
constexpr uint8_t kPortEEepromSda    = 0x04u;
constexpr uint8_t kPortEUsbConnect   = 0x10u;
constexpr uint8_t kPortEQuickLaunch1 = 0x40u;
constexpr uint8_t kPortEWiredInputs  = kPortEEepromSda | kPortEUsbConnect | kPortEQuickLaunch1;
constexpr uint8_t kPortGQuickLaunch4 = 0x01u;
constexpr uint8_t kPortGQuickLaunch3 = 0x02u;
constexpr uint8_t kPortGQuickLaunch2 = 0x04u;
constexpr uint8_t kPortGUnreadInputs = 0xF8u;
constexpr uint8_t kPortGWiredInputs  = kPortGQuickLaunch4 | kPortGQuickLaunch3 | kPortGQuickLaunch2 | kPortGUnreadInputs;
constexpr uint32_t kQuickLaunchRowPin = 1u;
constexpr uint8_t kPortFButtons            = 0x1Fu;
constexpr uint8_t kPortFUnreadInput        = 0x20u;
constexpr uint8_t kPortFCardDetect         = 0x40u;
constexpr uint8_t kPortFCardStatusChange   = 0x80u;
constexpr uint8_t kPortFWiredInputs        =
    kPortFButtons | kPortFUnreadInput | kPortFCardDetect | kPortFCardStatusChange;
constexpr uint8_t kPortHIrq0Line     = 0x01u;
constexpr uint8_t kPortHOnOffLine    = 0x02u;
constexpr uint8_t kPortHSerialDcd    = 0x04u;
constexpr uint8_t kPortHIrq3Line     = 0x08u;
constexpr uint8_t kPortHIrq4Line     = 0x10u;
constexpr uint8_t kPortHUnreadInputs = 0x60u;
constexpr uint8_t kPortHWiredInputs  =
    kPortHIrq0Line | kPortHOnOffLine | kPortHSerialDcd | kPortHIrq3Line | kPortHIrq4Line | kPortHUnreadInputs;
constexpr uint8_t kPortScUnreadInput = 0x20u;
constexpr uint8_t kPortScSerialCts   = 0x80u;
constexpr uint8_t kPortScWiredInputs = kPortScUnreadInput | kPortScSerialCts;

class HpJornada540PortWiring : public Sh77xxPortWiring {
public:
    using Sh77xxPortWiring::Sh77xxPortWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

    void OnReady() override {
        eeprom_bus_    = &emu_.Get<HpJornada540EepromBus>();
        audio_request_ = &emu_.Get<HpJornada540AudioRequestLine>();
        buttons_       = &emu_.Get<HpJornada540Buttons>();
    }

    void TrackDrive(const Sh77xxPortData& ports) override {
        nets_.Track(ports);
        quick_launch_row_low_ = ports.Drive(kSh77xxPortD, kQuickLaunchRowPin) == PinDrive::kLow;
        eeprom_bus_->OnLines((nets_.Levels(kSh77xxPortJ) & kPortJEepromScl) != 0u, SdaLine());
    }

    uint8_t WiredInputs(uint32_t port) const override {
        switch (port) {
            case kSh77xxPortC:  return kPortCWiredInputs;
            case kSh77xxPortD:  return kPortDWiredInputs;
            case kSh77xxPortE:  return kPortEWiredInputs;
            case kSh77xxPortF:  return kPortFWiredInputs;
            case kSh77xxPortG:  return kPortGWiredInputs;
            case kSh77xxPortH:  return kPortHWiredInputs;
            case kSh77xxPortSc: return kPortScWiredInputs;
            default:            return 0u;
        }
    }

    uint8_t NetLevels(uint32_t port) const override {
        const uint8_t nets = nets_.Levels(port);
        if (port == kSh77xxPortD) return Level(nets, kPortDAudioRequest, !audio_request_->Asserted());
        if (port == kSh77xxPortE) {
            const uint8_t sda = Level(nets, kPortEEepromSda, SdaLine());
            return QuickLaunchColumn(sda, kPortEQuickLaunch1, HpJornada540Button::kQuickLaunch1);
        }
        if (port == kSh77xxPortG) {
            uint8_t levels = QuickLaunchColumn(nets, kPortGQuickLaunch2, HpJornada540Button::kQuickLaunch2);
            levels         = QuickLaunchColumn(levels, kPortGQuickLaunch3, HpJornada540Button::kQuickLaunch3);
            return QuickLaunchColumn(levels, kPortGQuickLaunch4, HpJornada540Button::kQuickLaunch4);
        }
        return nets;
    }

    bool WiredDmaRequest(uint32_t channel) const override { return channel == kAudioRequestDreq; }

    void SaveState(StateWriter& w) override {
        nets_.SaveState(w);
        eeprom_bus_->SaveState(w);
    }

    void RestoreState(StateReader& r) override {
        nets_.RestoreState(r);
        eeprom_bus_->RestoreState(r);
    }

private:
    static uint8_t Level(uint8_t nets, uint8_t mask, bool high) {
        return static_cast<uint8_t>(high ? nets | mask : nets & ~mask);
    }

    uint8_t QuickLaunchColumn(uint8_t levels, uint8_t mask, HpJornada540Button button) const {
        return Level(levels, mask, !(quick_launch_row_low_ && buttons_->Pressed(button)));
    }

    bool SdaLine() const { return (nets_.Levels(kSh77xxPortE) & kPortEEepromSda) != 0u && eeprom_bus_->SdaReleased(); }

    HpJornada540EepromBus*                        eeprom_bus_    = nullptr;
    HpJornada540AudioRequestLine*                 audio_request_ = nullptr;
    HpJornada540Buttons*                          buttons_       = nullptr;
    bool                                          quick_launch_row_low_ = false;
    PinNetHold<kSh77xxPortCount, kSh77xxPortPins> nets_;
};

}

REGISTER_SERVICE_AS(HpJornada540PortWiring, Sh77xxPortWiring);

}
