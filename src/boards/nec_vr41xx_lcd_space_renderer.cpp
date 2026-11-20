#define NOMINMAX

#include "nec_vr41xx_lcd_space.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../host/panel_frame_renderer.h"
#include "../lcd/panel_scanout.h"

namespace cerfcore {

namespace {

class NecVr41xxLcdSpaceRenderer : public PanelFrameRenderer {
public:
    using PanelFrameRenderer::PanelFrameRenderer;

    bool ShouldRegister() override { return emu_.TryGet<NecVr41xxLcdPanel>() != nullptr; }

    void PresentedSize(uint32_t& w, uint32_t& h) override {
        auto& lcd = emu_.Get<NecVr41xxLcdSpace>();
        w = lcd.GuestW();
        h = lcd.GuestH();
    }

    bool HasFrame() override { return emu_.Get<NecVr41xxLcdSpace>().DisplayOn(); }

    void RenderInto(uint32_t* dib, uint32_t host_w, uint32_t host_h) override {
        auto& lcd = emu_.Get<NecVr41xxLcdSpace>();
        const PanelSurface src{.fb     = lcd.FbBytes(),
                               .stride = lcd.StrideBytes(),
                               .width  = lcd.GuestW(),
                               .height = lcd.GuestH()};
        scanout_.Blit(src, dib, host_w, host_h);
    }

private:
    PanelScanout scanout_{PanelGrayLayout::kPacked2Msb, {0xFFu, 0xAAu, 0x55u, 0x00u}};
};

}

REGISTER_SERVICE_AS(NecVr41xxLcdSpaceRenderer, PanelFrameRenderer);

}
