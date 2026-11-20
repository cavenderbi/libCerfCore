#include "../sh77xx/sh77xx_scif.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hd6417096_id.h"
#include "hd6417096_scif_channel.h"

namespace cerfcore {

namespace {

constexpr Sh77xxScifChannel kHd6417096ScifChannel{kHd6417096ScifBase,
                                                  "SCIF",
                                                  Sh77xxIntcSource::kIntevt700,
                                                  Sh77xxIntcSource::kIntevt720,
                                                  Sh77xxIntcSource::kIntevt740,
                                                  Sh77xxIntcSource::kIntevt760,
                                                  0x00u,
                                                  0u,
                                                  false,
                                                  0x00u,
                                                  true,
                                                  0u};

class Hd6417096Scif : public Sh77xxScif {
public:
    using Sh77xxScif::Sh77xxScif;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }

protected:
    const Sh77xxScifChannel& Channel() const override { return kHd6417096ScifChannel; }
};

}

REGISTER_SERVICE(Hd6417096Scif);

}
