#include "siemens_simatic_mp377_panel.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/board_database.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../core/board_parts.h"
#include "../../core/lcd_panel_generic_part_id.h"
#include "siemens_simatic_mp377_family_id.h"

namespace cerfcore {

namespace siemens_simatic_mp377 {

REGISTER_SERVICE(SiemensSimaticMp377Panel);

namespace {

constexpr SiemensSimaticMp377PanelProfile kProfiles[] = {
    SiemensSimaticMp377PanelProfile::Inch12_800x600,
    SiemensSimaticMp377PanelProfile::Inch15_1024x768,
    SiemensSimaticMp377PanelProfile::Inch19_1280x1024,
};

}

bool SiemensSimaticMp377Panel::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SiemensSimaticMp377;
}

void SiemensSimaticMp377Panel::OnReady() {
    const DbPart& panel = *emu_.Get<BoardParts>().Find(GenericPartId::LcdPanel);
    for (SiemensSimaticMp377PanelProfile p : kProfiles) {
        if (Mp377PanelWidth(p) == *panel.width && Mp377PanelHeight(p) == *panel.height) {
            profile_ = p;
            return;
        }
    }
    emu_.Get<Fatal>().Die("SiemensSimaticMp377Panel: no panel profile for %ux%u (%s)", *panel.width, *panel.height,
                          panel.id.c_str());
}

}

}
