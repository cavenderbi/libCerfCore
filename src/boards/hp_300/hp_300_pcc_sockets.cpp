#include "../../socs/sh7707/sh7707_pcc_sockets.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hp_300_family_id.h"
#include "hp_300_member_straps.h"

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

    void OnReady() override { straps_ = &emu_.Get<Hp300MemberStraps>(); }

    bool Fitted(uint32_t channel) const override {
        return channel == kPcCardChannel || !(straps_->Luke() && !straps_->Model2());
    }

    ObjectIdentity Socket(uint32_t channel) const override {
        if (channel == kPcCardChannel) return {kPcCardSlotId, kPcCardSlotLabel};
        return {kCfSlotId, kCfSlotLabel};
    }

private:
    const Hp300MemberStraps* straps_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp300PccSockets, Sh7707PccSockets);

}
