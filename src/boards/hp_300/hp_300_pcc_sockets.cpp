#include "../../socs/sh7707/sh7707_pcc_sockets.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../core/board_parts.h"
#include "hp_300_compact_flash_part_id.h"
#include "hp_300_family_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kPcCardChannel = 0u;

constexpr char kPcCardSlotId[]    = "pc_card_slot";
constexpr char kPcCardSlotLabel[] = "PC Card slot";
constexpr char kCfSlotId[]        = "cf_slot";
constexpr char kCfSlotLabel[]     = "CF slot";

class Hp300PccSockets : public Sh7707PccSockets {
public:
    using Sh7707PccSockets::Sh7707PccSockets;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override { cf_fitted_ = emu_.Get<BoardParts>().Has(PartId::Hp300CompactFlash); }

    bool Fitted(uint32_t channel) const override { return channel == kPcCardChannel || cf_fitted_; }

    ObjectIdentity Socket(uint32_t channel) const override {
        if (channel == kPcCardChannel) return {kPcCardSlotId, kPcCardSlotLabel};
        return {kCfSlotId, kCfSlotLabel};
    }

private:
    bool cf_fitted_ = false;
};

}

REGISTER_SERVICE_AS(Hp300PccSockets, Sh7707PccSockets);

}
