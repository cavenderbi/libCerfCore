#include "hp_300_touch_panel.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../peripherals/pin_drive.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "hp_300_family_id.h"

namespace cerfcore {

REGISTER_SERVICE(Hp300TouchPanel);

namespace {

constexpr uint32_t kPortHPlateDrivePin = 7u;
constexpr uint32_t kPortKXPlatePin     = 0u;
constexpr uint32_t kPortKYPlatePin     = 2u;
constexpr uint16_t kAdcFullScale       = 0x3FFu;
constexpr uint16_t kDetectPenDown      = kAdcFullScale;
constexpr uint16_t kDetectPenUp        = 0u;
constexpr uint16_t kDetectYPlate       = 0u;

constexpr HostPenInput::PanelRange kScreen{0u, kAdcFullScale, 0u, kAdcFullScale};

}

bool Hp300TouchPanel::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
}

uint16_t Hp300TouchPanel::ConvertPlate(Plate plate) {
    const PinDrive plate_drive = Ports().Drive(kSh77xxPortH, kPortHPlateDrivePin);
    const PinDrive x_plate     = Ports().Drive(kSh77xxPortK, kPortKXPlatePin);
    const PinDrive y_plate     = Ports().Drive(kSh77xxPortK, kPortKYPlatePin);
    const bool     detect      = plate_drive == PinDrive::kLow && x_plate == PinDrive::kHigh && y_plate == PinDrive::kHigh;
    const bool     measure_x = plate_drive == PinDrive::kHigh && x_plate == PinDrive::kLow && y_plate == PinDrive::kHigh;
    const bool     measure_y = plate_drive == PinDrive::kHigh && x_plate == PinDrive::kHigh && y_plate == PinDrive::kLow;
    if (detect) {
        if (plate == Plate::kX) return PenLineHigh() ? kDetectPenDown : kDetectPenUp;
        return kDetectYPlate;
    }
    if ((measure_x && plate == Plate::kX) || (measure_y && plate == Plate::kY)) return PlatePosition(kScreen, plate);
    emu_.Get<Fatal>().Die("Hp300TouchPanel: %s plate conversion with PH7 drive %u, PK0 drive %u, PK2 drive %u",
                          plate == Plate::kX ? "X" : "Y", static_cast<unsigned>(plate_drive),
                          static_cast<unsigned>(x_plate), static_cast<unsigned>(y_plate));
}

}
