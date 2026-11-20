#include "casio_cassiopeia_a10_lcd.h"
#include "casio_cassiopeia_a10_video_memory.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/emulated_memory.h>
#include "../../host/panel_frame_renderer.h"
#include "../../lcd/panel_scanout.h"
#include "casio_cassiopeia_a10_family_id.h"

namespace cerfcore {

namespace {

constexpr PanelGrayLevels kPaletteLevels = {0x00u, 0x80u, 0xC0u, 0xFFu};

class CasioCassiopeiaA10Renderer : public PanelFrameRenderer {
public:
    using PanelFrameRenderer::PanelFrameRenderer;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override { video_memory_ = emu_.TryGet<CasioCassiopeiaA10VideoMemory>(); }

    void PresentedSize(uint32_t& w, uint32_t& h) override {
        w = CasioCassiopeiaA10Lcd::kWidth;
        h = CasioCassiopeiaA10Lcd::kHeight;
    }

    bool HasFrame() override { return emu_.Get<CasioCassiopeiaA10Lcd>().PanelOn(); }

    void RenderInto(uint32_t* dib, uint32_t host_w, uint32_t host_h) override {
        if (video_memory_ != nullptr) {
            const PanelSurface src{.fb     = video_memory_->Frame(),
                                   .stride = CasioCassiopeiaA10VideoMemory::kRowStride,
                                   .width  = CasioCassiopeiaA10Lcd::kWidth,
                                   .height = CasioCassiopeiaA10Lcd::kHeight};
            packed_.Blit(src, dib, host_w, host_h);
            return;
        }
        auto&          lcd = emu_.Get<CasioCassiopeiaA10Lcd>();
        constexpr auto kPlaneBytes =
            static_cast<uint64_t>(CasioCassiopeiaA10Lcd::kPlaneStride) * CasioCassiopeiaA10Lcd::kHeight;
        const uint8_t* fb = emu_.Get<EmulatedMemory>().TryTranslateRange(
            lcd.FramePa(), kPlaneBytes * CasioCassiopeiaA10Lcd::kShownPlanes);
        const PanelSurface src{.fb     = fb,
                               .stride = CasioCassiopeiaA10Lcd::kPlaneStride,
                               .width  = CasioCassiopeiaA10Lcd::kWidth,
                               .height = CasioCassiopeiaA10Lcd::kHeight};
        planes_.Blit(src, dib, host_w, host_h);
    }

private:
    CasioCassiopeiaA10VideoMemory* video_memory_ = nullptr;
    PanelScanout                   planes_{PanelGrayLayout::kPlanes2Lsb, kPaletteLevels};
    PanelScanout                   packed_{PanelGrayLayout::kPacked2Msb, kPaletteLevels};
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10Renderer, PanelFrameRenderer);

}
