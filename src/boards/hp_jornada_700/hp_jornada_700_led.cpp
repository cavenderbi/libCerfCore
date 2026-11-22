#include "hp_jornada_700_led.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/state/emulation_freeze.h>
#include <cerfcore/state/hibernation.h>
#include "../../socs/sa11xx/sa11xx_gpio.h"
#include <cerfcore/boards/board_context.h>
#include "hp_jornada_700_family_id.h"

namespace cerfcore {

REGISTER_SERVICE(HpJornada700Led);

namespace {
constexpr uint32_t  kBlinkHalfMs = 500;
constexpr ObjectRgb kLedColor{120, 255, 140};
constexpr uint32_t  kButtonPin   = 13;
}

bool HpJornada700Led::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetBoardFamilyId() == BoardFamilyId::HpJornada700;
}

void HpJornada700Led::OnReady() {
    DriveButtonLine(false);
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { DriveButtonLine(false); });
    auto& tree = emu_.Get<MachineObjectTree>();
    tree.Add(this);
    tree.Add(&button_, this);
}

void HpJornada700Led::SetState(State s) {
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        state_ = s;
    }
    Refresh();
}

uint8_t HpJornada700Led::StatusByte() const {
    switch (CurrentState()) {
        case State::On:    return 0x01;
        case State::Blink: return 0x03;
        default:           return 0x00;
    }
}

HpJornada700Led::State HpJornada700Led::CurrentState() const {
    std::lock_guard<std::mutex> lk(state_mutex_);
    return state_;
}

void HpJornada700Led::DriveButton(bool pressed) {
    auto frozen = emu_.Get<EmulationFreeze>().WorkerSection();
    DriveButtonLine(pressed);
    LOG(Periph, "[J720LED] button %s, GPIO edges 0x%08X\n", pressed ? "down" : "up",
        emu_.Get<Sa11xxGpio>().InputEdges());
}

void HpJornada700Led::DriveButtonLine(bool pressed) { emu_.Get<Sa11xxGpio>().DriveInputPin(kButtonPin, !pressed); }

LampState HpJornada700Led::Lamp() const {
    const State s     = CurrentState();
    const bool  blink = s == State::Blink;
    return LampState{s != State::Off, blink, kLedColor, blink ? kBlinkHalfMs : 0u, blink ? kBlinkHalfMs : 0u};
}

}
