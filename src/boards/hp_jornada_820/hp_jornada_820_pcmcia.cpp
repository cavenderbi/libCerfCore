#include "hp_jornada_820_pcmcia.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "../../peripherals/pcmcia/pcmcia_space_router.h"
#include <cerfcore/boards/board_context.h>
#include "hp_jornada_820_family_id.h"
#include "hp_jornada_820_companion_asic.h"

namespace cerfcore {

namespace {

constexpr char kPcCardSlotId[]    = "pc_card_slot";
constexpr char kPcCardSlotLabel[] = "PC Card slot";
constexpr char kCfSlotId[]        = "cf_slot";
constexpr char kCfSlotLabel[]     = "CompactFlash slot";

constexpr uint32_t kS0Ncd = 0x4u,    kS0Rdy = 0x1u,
                   kS0Bvd1 = 0x400u, kS0Bvd2 = 0x800u;
constexpr uint32_t kS1Ncd = 0x8u,    kS1Rdy = 0x2u,
                   kS1Bvd1 = 0x1000u, kS1Bvd2 = 0x2000u;

}  /* namespace */

HpJornada820Pcmcia::HpJornada820Pcmcia(CerfEmulator& emu)
    : Service(emu),
      slot0_(emu, *this, {kPcCardSlotId, kPcCardSlotLabel}),
      slot1_(emu, *this, {kCfSlotId, kCfSlotLabel}) {}

bool HpJornada820Pcmcia::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada820;
}

void HpJornada820Pcmcia::OnReady() {
    emu_.Get<PcmciaSpaceRouter>().ProvideSockets(&slot0_, &slot1_);
    auto& tree = emu_.Get<MachineObjectTree>();
    tree.Add(&slot0_);
    tree.Add(&slot1_);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
        if (kind != ResetLineKind::Rtc) return;
        slot0_.PowerCycleCard();
        slot1_.PowerCycleCard();
    });
}

void HpJornada820Pcmcia::OnShutdown() {
    slot0_.OnShutdown();
    slot1_.OnShutdown();
}

uint32_t HpJornada820Pcmcia::ReadSocketStatus() const {
    uint32_t s = kS0Bvd2 | kS1Bvd2;
    if (!slot0_.StatusChangeLow()) s |= kS0Bvd1;
    if (!slot1_.StatusChangeLow()) s |= kS1Bvd1;
    s |= slot0_.HasCard() ? kS0Rdy : kS0Ncd;
    s |= slot1_.HasCard() ? kS1Rdy : kS1Ncd;
    return s;
}

void HpJornada820Pcmcia::OnCardDetectChanged(PcmciaSlot& slot) {
    /* Power on presence, NOT the control-reg Vcc bit: ne2000.dll DetectNE2000
       writes config regs + probes card I/O before the PDD's SetSocket Vcc, so
       gating power on that bit leaves the card unpowered during detection and
       it reads back 0xFF -> "Unidentified PCCard". */
    slot.SetPowered(slot.HasCard());
    /* A runtime insert/eject must raise the companion status-change interrupt
       (SYSINTR 27) or the PDD detect poller (pcmcia.dll sub_12A59E8), which
       waits INFINITE on that interrupt, never re-scans the socket. */
    const int socket = (&slot == &slot1_) ? 1 : 0;
    emu_.Get<HpJornada820CompanionAsic>().RaisePcmciaStatusChange(socket);
}

void HpJornada820Pcmcia::OnReadyIreqLow(PcmciaSlot& slot) {
    const int socket = (&slot == &slot1_) ? 1 : 0;
    emu_.Get<HpJornada820CompanionAsic>().RaisePcmciaCardIrq(socket);
}
void HpJornada820Pcmcia::OnReadyIreqHigh(PcmciaSlot&) {}

void HpJornada820Pcmcia::OnStatusChange(PcmciaSlot& slot, bool low) {
    if (!low) return;
    emu_.Get<HpJornada820CompanionAsic>().RaisePcmciaStatusChange(&slot == &slot1_ ? 1 : 0);
}

REGISTER_SERVICE(HpJornada820Pcmcia);

}
