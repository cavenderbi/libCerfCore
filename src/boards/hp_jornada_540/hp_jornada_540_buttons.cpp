#include "hp_jornada_540_buttons.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfcore/state/hibernation.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "hp_jornada_540_family_id.h"
#include "hp_jornada_540_key_wake.h"

#include <iterator>

namespace cerfcore {

REGISTER_SERVICE(HpJornada540Buttons);

namespace {

constexpr uint32_t Bit(HpJornada540Button button) { return 1u << static_cast<uint32_t>(button); }

constexpr uint32_t kQuickLaunchBits = Bit(HpJornada540Button::kQuickLaunch1) | Bit(HpJornada540Button::kQuickLaunch2) |
                                      Bit(HpJornada540Button::kQuickLaunch3) | Bit(HpJornada540Button::kQuickLaunch4);

constexpr uint32_t kWakeKeyBits = kQuickLaunchBits | Bit(HpJornada540Button::kRecord);

constexpr Sh77xxIntcPin kPortFButtonPins[] = {
    Sh77xxPintPin(8u), Sh77xxPintPin(9u), Sh77xxPintPin(10u), Sh77xxPintPin(11u), Sh77xxPintPin(12u),
};

}

bool HpJornada540Buttons::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

void HpJornada540Buttons::OnReady() {
    intc_     = &emu_.Get<Sh77xxIntc>();
    key_wake_ = &emu_.Get<HpJornada540KeyWake>();
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { ReleaseAll(); });
    emu_.Get<GuestDeepSleep>().RegisterUserWakeInput([this] {
        intc_->SetPinLevel(Sh77xxIntcPin::kIrq1, false);
        Publish(HpJornada540Button::kOnOff);
    });
    ReleaseAll();
}

void HpJornada540Buttons::SetPressed(HpJornada540Button button, bool pressed) {
    const uint32_t bit = Bit(button);
    const uint32_t was = pressed ? pressed_.fetch_or(bit, std::memory_order_acq_rel)
                                 : pressed_.fetch_and(~bit, std::memory_order_acq_rel);
    Publish(button);
    if (pressed && (was & bit) == 0u && (bit & kWakeKeyBits) != 0u) key_wake_->Raise();
}

bool HpJornada540Buttons::Pressed(HpJornada540Button button) const {
    return (pressed_.load(std::memory_order_acquire) & Bit(button)) != 0u;
}

void HpJornada540Buttons::Publish(HpJornada540Button button) {
    const uint32_t index = static_cast<uint32_t>(button);
    if (index < std::size(kPortFButtonPins)) {
        intc_->SetPinLevel(kPortFButtonPins[index], !Pressed(button));
    } else if (button == HpJornada540Button::kOnOff) {
        intc_->SetPinLevel(Sh77xxIntcPin::kIrq1, !Pressed(button));
    } else {
        PublishQuickLaunch();
    }
}

void HpJornada540Buttons::PublishQuickLaunch() {
    const bool any = (pressed_.load(std::memory_order_acquire) & kQuickLaunchBits) != 0u;
    intc_->SetPinLevel(Sh77xxIntcPin::kIrq4, any);
}

void HpJornada540Buttons::ReleaseAll() {
    pressed_.store(0u, std::memory_order_release);
    for (uint32_t i = 0; i < static_cast<uint32_t>(HpJornada540Button::kCount); ++i) {
        Publish(static_cast<HpJornada540Button>(i));
    }
}

}
