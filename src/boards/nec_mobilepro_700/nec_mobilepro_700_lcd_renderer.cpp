#define NOMINMAX

#include "nec_mobilepro_700_lcd.h"

#include <cerfcore/boards/board_context.h>
#include "nec_mobilepro_700_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/panel_frame_renderer.h>
#include "../../lcd/panel_scanout.h"

namespace cerfcore {

namespace {

class NecMobilepro700LcdRenderer : public PanelFrameRenderer {
public:
    using PanelFrameRenderer::PanelFrameRenderer;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::NecMobilepro700;
    }

    void PresentedSize(uint32_t& w, uint32_t& h) override {
        auto& lcd = emu_.Get<NecMobilepro700Lcd>();
        w = lcd.GuestW();
        h = lcd.GuestH();
    }

    bool HasFrame() override {
        auto& lcd = emu_.Get<NecMobilepro700Lcd>();
        if (!lcd.IsEnabled())  return false;
        if (latch_.Latched())  return true;
        const uint32_t bytes = lcd.StrideBytes() * lcd.GuestH();
        return latch_.ProbeAndLatch(lcd.FbBytes(), bytes);
    }

    void RenderInto(uint32_t* dib, uint32_t host_w, uint32_t host_h) override {
        auto& lcd = emu_.Get<NecMobilepro700Lcd>();
        const PanelSurface src{.fb     = lcd.FbBytes(),
                               .stride = lcd.StrideBytes(),
                               .width  = lcd.GuestW(),
                               .height = lcd.GuestH()};
        scanout_.Blit(src, dib, host_w, host_h);
    }

private:
    PanelScanout scanout_{PanelPixelFormat::kGray2MsbInverted};
};

}

REGISTER_SERVICE_AS(NecMobilepro700LcdRenderer, PanelFrameRenderer);

}
