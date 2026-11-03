#pragma once

#include "../socs/vr41xx/vr41xx_piu_panel.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

class NecVr41xxTouchPanel : public Vr41xxPiuPanel {
public:
    using Vr41xxPiuPanel::Vr41xxPiuPanel;

    std::optional<uint16_t> ConvertCommandPort(uint16_t adcmd, uint16_t pos_x, uint16_t pos_y) override;
    std::optional<uint16_t> PressureSample() override;
    std::optional<uint16_t> AdPortScanSample(uint16_t port) override;

protected:
    virtual uint16_t MainBatteryWord() const = 0;
    virtual uint16_t BackupCellWord() const = 0;
};

}
