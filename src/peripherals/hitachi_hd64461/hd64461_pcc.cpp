#include "hd64461_pcc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../boards/hp_600/hp_600_family_id.h"
#include "hd64461_gpio.h"
#include "hd64461_intc.h"
#include "hd64461_socket_wiring.h"
#include "hd64461_system_control.h"

namespace cerfcore {

REGISTER_SERVICE(Hd64461Pcc);

namespace {

constexpr uint32_t kScr = 4u;

constexpr uint16_t kGcrVccSel0      = 0x10u;
constexpr uint16_t kScrVccSel1      = 0x02u;
constexpr uint16_t kScrWriteProtect = 0x01u;

}

bool Hd64461Pcc::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
}

void Hd64461Pcc::OnReady() {
    intc_   = &emu_.Get<Hd64461Intc>();
    gpio_   = &emu_.Get<Hd64461Gpio>();
    wiring_ = &emu_.Get<Hd64461SocketWiring>();
    system_ = &emu_.Get<Hd64461SystemControl>();
    HitachiPccBase::OnReady();
}

void Hd64461Pcc::ApplyInputGating(uint32_t gated_ports) {
    std::lock_guard<std::mutex> lk(lock_);
    gpio_->GateInputs(gated_ports, PinsLocked(1u));
    RefreshChannelLocked(1u);
}

ObjectIdentity Hd64461Pcc::SocketIdentity(uint32_t ch) const { return wiring_->Socket(ch); }

bool Hd64461Pcc::SocketFitted(uint32_t ch) const {
    (void)ch;
    return true;
}

bool Hd64461Pcc::SocketPoweredLocked(uint32_t ch) const {
    const uint32_t base = ch * hitachi_pcc_detail::kChannelRegs;
    return wiring_->SocketPowered(ch, (StoredReg(base + hitachi_pcc_detail::kGcr) & kGcrVccSel0) != 0u,
                                  (StoredReg(base + kScr) & kScrVccSel1) != 0u);
}

bool Hd64461Pcc::WriteProtectedLocked(uint32_t ch) const {
    return (StoredReg(ch * hitachi_pcc_detail::kChannelRegs + kScr) & kScrWriteProtect) != 0u;
}

HitachiPccSocketPins Hd64461Pcc::PresentLocked(uint32_t ch, const HitachiPccSocketPins& live) const {
    return ch == 1u ? gpio_->Pcmcia1Presented(live) : live;
}

void Hd64461Pcc::DeliverCausesLocked(uint32_t ch, uint32_t causes) {
    intc_->SetSourceRequest(ch == 0u ? Hd64461IntcSource::kPcc0 : Hd64461IntcSource::kPcc1, causes != 0u);
}

void Hd64461Pcc::RequireCardCycle(uint32_t ch, uint32_t offset) {
    if (!system_->PccChannelClockRunning(ch)) {
        emu_.Get<Fatal>().Die("Hd64461Pcc: card cycle at offset 0x%08X to PCC%u while the channel is in standby", offset,
                              ch);
    }
    if (ch == 1u && !gpio_->Pcmcia1PinsConnected()) {
        emu_.Get<Fatal>().Die("Hd64461Pcc: card cycle at offset 0x%08X to PCC1 while GPCCR or GPDCR puts a PCC1 "
                              "output pin in port mode", offset);
    }
}

}
