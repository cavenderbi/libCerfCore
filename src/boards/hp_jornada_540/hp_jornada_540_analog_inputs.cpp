#include "../../socs/sh77xx/sh77xx_analog_inputs.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "../sh77xx_adc_microphone.h"
#include "hp_jornada_540_battery.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kMicrophoneChannel  = 0u;
constexpr uint32_t kMainBatteryChannel = 5u;
constexpr uint8_t  kPortDRecordEnable  = 0x20u;
constexpr uint8_t  kPortERecordEnable  = 0x80u;

class HpJornada540AnalogInputs : public Sh77xxAnalogInputs {
public:
    using Sh77xxAnalogInputs::Sh77xxAnalogInputs;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

    void OnReady() override {
        battery_    = &emu_.Get<HpJornada540Battery>();
        microphone_ = &emu_.Get<Sh77xxAdcMicrophone>();
        wiring_     = &emu_.Get<Sh77xxPortWiring>();
    }

    uint16_t Convert(uint32_t channel) override {
        if (channel == kMicrophoneChannel) return ConvertMicrophone();
        if (channel == kMainBatteryChannel) return battery_->MainBatteryCode();
        emu_.Get<Fatal>().Die("HpJornada540AnalogInputs: A/D conversion of AN%u", channel);
    }

private:
    uint16_t ConvertMicrophone() {
        const uint8_t port_d = wiring_->NetLevels(kSh77xxPortD);
        const uint8_t port_e = wiring_->NetLevels(kSh77xxPortE);
        if ((port_d & kPortDRecordEnable) == 0u || (port_e & kPortERecordEnable) == 0u) {
            emu_.Get<Fatal>().Die("HpJornada540AnalogInputs: A/D conversion of AN0 with port D nets 0x%02X and port E "
                                  "nets 0x%02X",
                                  port_d, port_e);
        }
        return microphone_->Convert();
    }

    HpJornada540Battery* battery_    = nullptr;
    Sh77xxAdcMicrophone* microphone_ = nullptr;
    Sh77xxPortWiring*    wiring_     = nullptr;
};

}

REGISTER_SERVICE_AS(HpJornada540AnalogInputs, Sh77xxAnalogInputs);

}
