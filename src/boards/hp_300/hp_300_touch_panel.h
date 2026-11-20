#pragma once

#include "../sh77xx_pen_line_panel.h"

#include <cstdint>

namespace cerfcore {

class Hp300TouchPanel : public Sh77xxPenLinePanel {
public:
    using Sh77xxPenLinePanel::Sh77xxPenLinePanel;

    bool ShouldRegister() override;

    uint16_t ConvertPlate(Plate plate);

protected:
    Sh77xxIntcPin PenLine() const override { return Sh77xxIntcPin::kIrq3; }
};

}
