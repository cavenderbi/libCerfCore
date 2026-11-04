#include "../../peripherals/pcmcia/pcmcia_space_router.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "pxa255_id.h"

namespace cerfcore {

namespace {

class Pxa255PcmciaSpaceRouter : public PcmciaSpaceRouter {
public:
    using PcmciaSpaceRouter::PcmciaSpaceRouter;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == SocId::Pxa255;
    }

    FastReadFn  FastReader() override { return &Peripheral::SplitWordRead; }
    FastWriteFn FastWriter() override { return &Peripheral::SplitWordWrite; }
};

}

REGISTER_SERVICE_AS(Pxa255PcmciaSpaceRouter, PcmciaSpaceRouter);

}
