#define NOMINMAX

#include "mediaq_mq1188.h"

#include "../mediaq_ge/mediaq_panel_renderer.h"
#include "../../core/board_parts.h"
#include "mediaq_mq1188_part_id.h"

namespace cerfcore {

namespace {

class MediaQMq1188Renderer : public MediaQPanelRenderer<MediaQMq1188> {
public:
    using MediaQPanelRenderer<MediaQMq1188>::MediaQPanelRenderer;

    bool ShouldRegister() override {
        return emu_.Get<BoardParts>().Has(PartId::MediaqMq1188);
    }
};

}

REGISTER_SERVICE_AS(MediaQMq1188Renderer, PanelFrameRenderer);

}
