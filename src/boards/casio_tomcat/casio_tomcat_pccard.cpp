#include "casio_tomcat_pccard.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/host_widget_registry.h>
#include "../../peripherals/pcmcia/pcmcia_auto_insert.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include "../../socs/vr41xx/vr41xx_giu.h"
#include <cerfcore/state/state_stream.h>
#include <cerfcore/boards/board_context.h>
#include "casio_tomcat_family_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kWindowBase = 0x1400E000u;
constexpr uint32_t kWindowSize = 0x800u;

/* casio_cassiopeia_e55 pcmcia.dll sub_14C0B84 @0x14C0B84 reports a card on
   (+0x04 & 6) == 0 and ready on (+0x04 & 1) != 0. */
constexpr uint16_t kStatusCardAbsent = 0x0006u;
constexpr uint16_t kStatusReady      = 0x0001u;

/* casio_cassiopeia_e55 pcmcia.dll sub_14C0B84 @0x14C0B84 turns a CLEAR +0x04 D3 into a set
   bit of its own status byte; no function of the module writes D3 or branches on that
   status bit. */
constexpr uint16_t kStatusD3 = 0x0008u;

/* casio_cassiopeia_e55 pcmcia.dll sub_14C0CAC @0x14C0CAC gates GIUINTENL D0 on the socket
   record's byte 5 bit 7 and masks D1 unconditionally; sub_14C1078 @0x14C1078 does the same.
   Both clear GIUINTALSELL D0, a low-active level input (PC Card Standard IREQ#). */
constexpr int kCardIreqGiuPin = 0;

}  /* namespace */

CasioTomcatPcCard::CasioTomcatPcCard(CerfEmulator& emu)
    : Peripheral(emu), slot0_(emu, *this, "CompactFlash slot") {}

bool CasioTomcatPcCard::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioTomcat;
}

void CasioTomcatPcCard::OnReady() {
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<HostWidgetRegistry>().Register(&slot0_);
    emu_.Get<Vr41xxGiu>().SetPinLevel(kCardIreqGiuPin, true);
    emu_.Get<PcmciaAutoInsert>().InsertLaunchCompactFlash(slot0_);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
        reg_space_ = false;
    });
    emu_.Get<GuestCpuReset>().RegisterResetReleaseListener([this] {
        slot0_.SetPowered(false);
        slot0_.SetResetLine(false);
        emu_.Get<Vr41xxGiu>().SetPinLevel(kCardIreqGiuPin, true);
    });
}

void CasioTomcatPcCard::OnShutdown() { slot0_.OnShutdown(); }

uint32_t CasioTomcatPcCard::MmioBase() const { return kWindowBase; }
uint32_t CasioTomcatPcCard::MmioSize() const { return kWindowSize; }

uint8_t CasioTomcatPcCard::ReadByte(uint32_t addr) {
    const uint32_t off = addr - kWindowBase;
    if (reg_space_) return slot0_.ReadAttribute8(off);
    return slot0_.ReadCommon8(off);
}

void CasioTomcatPcCard::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off = addr - kWindowBase;
    if (reg_space_) { slot0_.WriteAttribute8(off, value); return; }
    slot0_.WriteCommon8(off, value);
}

uint16_t CasioTomcatPcCard::ReadHalf(uint32_t addr) {
    if (reg_space_) return Peripheral::ReadHalf(addr);
    return slot0_.ReadCommon16(addr - kWindowBase);
}

void CasioTomcatPcCard::WriteHalf(uint32_t addr, uint16_t value) {
    if (reg_space_) { Peripheral::WriteHalf(addr, value); return; }
    slot0_.WriteCommon16(addr - kWindowBase, value);
}

uint8_t  CasioTomcatPcCard::ReadIo8 (uint32_t off) { return slot0_.ReadIo8(off); }
uint16_t CasioTomcatPcCard::ReadIo16(uint32_t off) { return slot0_.ReadIo16(off); }
void CasioTomcatPcCard::WriteIo8 (uint32_t off, uint8_t  v) { slot0_.WriteIo8(off, v); }
void CasioTomcatPcCard::WriteIo16(uint32_t off, uint16_t v) { slot0_.WriteIo16(off, v); }

void CasioTomcatPcCard::SetRegSpace(bool on) { reg_space_ = on; }

void CasioTomcatPcCard::SetSocketPower(bool on) { slot0_.SetPowered(on); }

void CasioTomcatPcCard::SetResetStrobe(bool on) {
    slot0_.SetResetLine(on);
}

uint16_t CasioTomcatPcCard::StatusReg() const {
    uint16_t v = kStatusD3;
    if (!slot0_.HasCard()) v |= kStatusCardAbsent;
    if (slot0_.IsPowered() && !ready_ireq_low_.load(std::memory_order_acquire)) v |= kStatusReady;
    return v;
}

/* casio_cassiopeia_e55 pcmcia.dll sub_14C091C @0x14C091C starts sub_14C0658 @0x14C0658,
   which samples (+0x04 & 6) once a second and calls SetInterruptEvent(21) on a change. */
void CasioTomcatPcCard::OnCardDetectChanged(PcmciaSlot&) {}

void CasioTomcatPcCard::OnReadyIreqLow(PcmciaSlot&) {
    ready_ireq_low_.store(true, std::memory_order_release);
    emu_.Get<Vr41xxGiu>().SetPinLevel(kCardIreqGiuPin, false);
}

void CasioTomcatPcCard::OnReadyIreqHigh(PcmciaSlot&) {
    ready_ireq_low_.store(false, std::memory_order_release);
    emu_.Get<Vr41xxGiu>().SetPinLevel(kCardIreqGiuPin, true);
}

void CasioTomcatPcCard::SaveState(StateWriter& w) {
    w.Write("reg_space", reg_space_);
    w.Write<uint8_t>("ready_ireq_low", ready_ireq_low_.load(std::memory_order_acquire) ? 1u : 0u);
    slot0_.SaveSlotState(w);
}

void CasioTomcatPcCard::RestoreState(StateReader& r) {
    r.Read("reg_space", reg_space_);
    uint8_t ready_ireq_low = 0u;
    r.Read("ready_ireq_low", ready_ireq_low);
    ready_ireq_low_.store(ready_ireq_low != 0u, std::memory_order_release);
    slot0_.RestoreSlotState(r);
}

void CasioTomcatPcCard::PostRestore() { slot0_.PostRestoreSlot(); }

REGISTER_SERVICE(CasioTomcatPcCard);

}
