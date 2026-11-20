#include "hp_600_touch_panel.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../peripherals/pin_drive.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "hp_600_family_id.h"

namespace cerfcore {

REGISTER_SERVICE(Hp600TouchPanel);

namespace {

constexpr HostPenInput::PanelRange kScreen{38u, 950u, 80u, 900u};
constexpr uint32_t kPlateXDrive = 0u;
constexpr uint32_t kPlateYDrive = 1u;
constexpr uint32_t kScanEnable  = 5u;
constexpr uint8_t  kRawTap      = HostPenInput::kFirstOwnerKind;
constexpr GuestCycleClock::Rate kRawTapHold{10u, 1u};

}

bool Hp600TouchPanel::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
}

void Hp600TouchPanel::OnReady() {
    raw_hold_.Attach([this] { ResetPen(); });
    Sh77xxPenLinePanel::OnReady();
}

void Hp600TouchPanel::ApplyPen(const HostPenInput::Pen& pen) {
    if (pen.kind != kRawTap) {
        raw_tap_ = false;
        raw_hold_.Stop();
        DrivePenLine(pen.kind == HostPenInput::kPenDown);
        return;
    }
    raw_x_   = static_cast<uint16_t>(pen.x);
    raw_y_   = static_cast<uint16_t>(pen.y);
    raw_tap_ = true;
    DrivePenLine(true);
    raw_hold_.Start(kRawTapHold);
}

void Hp600TouchPanel::ResetPen() {
    raw_tap_ = false;
    raw_hold_.Stop();
    DrivePenLine(false);
}

void Hp600TouchPanel::TapRaw(uint16_t adc_x, uint16_t adc_y) { QueuePen(HostPenInput::Pen{kRawTap, adc_x, adc_y}); }

uint16_t Hp600TouchPanel::ReadPlate(Plate plate) {
    const PinDrive x_drive = Ports().Drive(kSh77xxPortSc, kPlateXDrive);
    const PinDrive y_drive = Ports().Drive(kSh77xxPortSc, kPlateYDrive);
    const PinDrive scan    = Ports().Drive(kSh77xxPortSc, kScanEnable);
    const bool     y_plate = plate == Plate::kY;
    const PinDrive low     = y_plate ? y_drive : x_drive;
    const PinDrive high    = y_plate ? x_drive : y_drive;
    if (scan == PinDrive::kHigh && low == PinDrive::kLow && high == PinDrive::kHigh) {
        if (raw_tap_) return y_plate ? raw_y_ : raw_x_;
        return PlatePosition(kScreen, plate);
    }
    emu_.Get<Fatal>().Die("Hp600TouchPanel: %s plate read with SCP0 drive %u, SCP1 drive %u, SCP5 drive %u",
                          y_plate ? "Y" : "X", static_cast<unsigned>(x_drive), static_cast<unsigned>(y_drive),
                          static_cast<unsigned>(scan));
}

}
