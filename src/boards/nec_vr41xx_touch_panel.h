#pragma once

#include "../socs/vr41xx/vr41xx_piu_panel.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

class NecVr41xxTouchPanel : public Vr41xxPiuPanel {
public:
    using Vr41xxPiuPanel::Vr41xxPiuPanel;

    std::optional<uint16_t> PressureSample(bool contact) override;
    uint16_t                ReleasedPlateSample() override;
    std::optional<uint16_t> PortSample(uint16_t port) override;

protected:
    virtual uint16_t MainBatteryWord() const = 0;
    virtual uint16_t BackupCellWord() const = 0;
};

}
