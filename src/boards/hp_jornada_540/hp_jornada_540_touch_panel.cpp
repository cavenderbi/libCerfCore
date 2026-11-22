#include "hp_jornada_540_touch_panel.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../peripherals/pin_drive.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

REGISTER_SERVICE(HpJornada540TouchPanel);

namespace {

constexpr HostPenInput::PanelRange kScreen{0u, 0x3FFu, 0u, 0x3FFu};
constexpr uint32_t                 kPin0 = 0u;
constexpr uint32_t                 kPin1 = 1u;

struct PlateDrive {
    PinDrive sc0;
    PinDrive sc1;
    PinDrive pe0;
    PinDrive pe1;

    bool operator==(const PlateDrive&) const = default;
};

constexpr PlateDrive kXPlateDrive{PinDrive::kLow, PinDrive::kHigh, PinDrive::kHigh, PinDrive::kLow};
constexpr PlateDrive kYPlateDrive{PinDrive::kHigh, PinDrive::kLow, PinDrive::kLow, PinDrive::kHigh};

}

bool HpJornada540TouchPanel::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
}

uint16_t HpJornada540TouchPanel::ReadPlate(Plate plate) {
    const PlateDrive drive{Ports().Drive(kSh77xxPortSc, kPin0), Ports().Drive(kSh77xxPortSc, kPin1),
                           Ports().Drive(kSh77xxPortE, kPin0), Ports().Drive(kSh77xxPortE, kPin1)};
    const bool       x_plate = plate == Plate::kX;
    if (drive == (x_plate ? kXPlateDrive : kYPlateDrive)) return PlatePosition(kScreen, plate);
    emu_.Get<Fatal>().Die("HpJornada540TouchPanel: %s plate read with SC0 drive %u, SC1 drive %u, PE0 drive %u, "
                          "PE1 drive %u",
                          x_plate ? "X" : "Y", static_cast<unsigned>(drive.sc0), static_cast<unsigned>(drive.sc1),
                          static_cast<unsigned>(drive.pe0), static_cast<unsigned>(drive.pe1));
}

}
