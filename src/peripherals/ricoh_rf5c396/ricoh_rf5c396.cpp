#include "ricoh_rf5c396.h"

#include <cerfcore/boards/board_context.h>
#include "../../boards/nec_cronos3e/nec_cronos3e_family_id.h"
#include "../../boards/nec_mobilepro_700/nec_mobilepro_700_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/host/host_widget_registry.h>
#include <cerfcore/state/state_stream.h>
#include "../pcmcia/pcmcia_host_cycles.h"

namespace cerfcore {

namespace {

constexpr uint32_t kPortIndex = 0x0u;
constexpr int      kSockets   = 2;

constexpr unsigned kIndexBankShift = 6u;
constexpr uint8_t  kIndexSubMask   = 0x3Fu;

constexpr uint8_t kRegInterfaceStatus = 0x01u;
constexpr uint8_t kIfsGpiInverted     = 0x80u;

constexpr uint8_t kIntrCscToIntr    = 0x10u;
constexpr uint8_t kNoIrqSelect      = 0x00u;
constexpr uint8_t kStatusChangeCardDetect = 0x08u;
constexpr uint8_t kDataBit7         = 0x80u;

constexpr bool kUndrivenIrqNetLevel = true;

constexpr ExcaPartRules kExcaRules = [] {
    ExcaPartRules rules{};
    rules.ready_change_any_edge = false;
    rules.latch_requires_enable = false;
    rules.status_read_masked_by_enable = false;
    rules.auto_power = true;
    rules.card_inputs_high_without_vcc = true;
    rules.power_control_reserved = 0x00u;
    rules.power_reserved_field_a = 0x13u;
    rules.power_reserved_code_a = 0x13u;
    rules.power_reserved_field_b = 0x00u;
    rules.power_reserved_code_b = 0x00u;
    rules.window_enable_reserved = 0x00u;
    rules.memory_end_high_reserved = 0x00u;
    rules.io_offset_low_reserved = 0x01u;
    rules.window_page_registers = false;
    rules.socket_control_power = false;
    rules.stschg_change_any_edge = false;
    rules.battery_dead_pin = ExcaBatteryDeadPin::kBvd1Low;
    rules.battery_warning = ExcaBatteryWarning::kEnterBvd1HighBvd2Low;
    rules.outputs_require_card = false;
    rules.ring_indicate_masks_status_change = true;
    rules.irq_select_reserved = 0x2146u;
    return rules;
}();

constexpr uint8_t  kWinMemcs16FullDecode = 0x20u;
constexpr unsigned kMemcs16UpperDecode   = 17u;
constexpr unsigned kMemcs16FullDecode    = 12u;

}

RicohRf5c396::RicohRf5c396(CerfEmulator& emu)
    : Service(emu),
      slot0_(emu, *this, "PCMCIA #1"),
      slot1_(emu, *this, "PCMCIA #2"),
      exca0_(kExcaRules),
      exca1_(kExcaRules) {}

bool RicohRf5c396::FittedOnFamily(std::string_view family) {
    return family == BoardFamilyId::NecMobilepro700 || family == BoardFamilyId::NecCronos3e;
}

bool RicohRf5c396::ShouldRegister() {
    return FittedOnFamily(emu_.Get<BoardContext>().GetBoardFamilyId());
}

void RicohRf5c396::OnReady() {
    board_ = &emu_.Get<RicohRf5c396Board>();
    auto& reg = emu_.Get<HostWidgetRegistry>();
    reg.Register(&slot0_);
    reg.Register(&slot1_);
    sequencer_.Run([this] {
        return ExcaSocketUpdates{{ {&slot0_, ExcaWriteEffect::SocketOff()},
                                   {&slot1_, ExcaWriteEffect::SocketOff()} }};
    });
    auto& reset = emu_.Get<GuestCpuReset>();
    reset.RegisterResetListener([this](ResetLineKind) { ResetController(); });
    reset.RegisterResetReleaseListener([this] {
        std::lock_guard<std::mutex> lk(state_mutex_);
        UpdateIrqLineLocked();
    });
}

void RicohRf5c396::ResetController() {
    sequencer_.Run([this] {
        std::lock_guard<std::mutex> lk(state_mutex_);
        index_ = 0u;
        exca0_.Reset();
        exca1_.Reset();
        chip_.Reset();
        ApplyChipModesLocked();
        TrackGpiLocked(0);
        TrackGpiLocked(1);
        return ExcaSocketUpdates{{ {&slot0_, ExcaWriteEffect::SocketOff()},
                                   {&slot1_, ExcaWriteEffect::SocketOff()} }};
    });
}

void RicohRf5c396::ApplyChipModesLocked() {
    for (int b = 0; b < kSockets; ++b) {
        Exca(b).SetExplicitStatusClear(chip_.ExplicitStatusClear());
        Exca(b).SetConfigurationResetOnRemoval(chip_.ConfigurationResetOnRemoval(b));
    }
}

uint8_t RicohRf5c396::ReadPcicByte(uint32_t port) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    if (port == kPortIndex) {
        if ((index_ >> kIndexBankShift) >= kSockets)
            return floating_.Read8("RF5C396 index register", port);
        return index_;
    }
    const uint8_t result = ReadDataLocked(port);
    UpdateIrqLineLocked();
    return result;
}

void RicohRf5c396::WritePcicByte(uint32_t port, uint8_t value) {
    if (port == kPortIndex) {
        std::lock_guard<std::mutex> lk(state_mutex_);
        index_ = value;
        return;
    }
    sequencer_.Run([&] {
        ExcaSocketUpdates updates{};
        std::lock_guard<std::mutex> lk(state_mutex_);
        const int bank = index_ >> kIndexBankShift;
        if (bank >= kSockets)
            return updates;
        WriteDataLocked(bank, value, &updates[0].effect);
        updates[0].slot = &Slot(bank);
        UpdateIrqLineLocked();
        return updates;
    });
}

bool RicohRf5c396::GpiLowLocked(int bank) {
    return board_->GpiWiredToVs1(bank) && Slot(bank).Vs1Grounded();
}

void RicohRf5c396::TrackGpiLocked(int bank) {
    const bool low = GpiLowLocked(bank);
    if (low == gpi_low_[bank]) return;
    gpi_low_[bank] = low;
    if (chip_.GpiEnabled(bank) && low != chip_.GpiInterruptOnRise(bank))
        Exca(bank).LatchGpiChange();
}

uint8_t RicohRf5c396::ReadDataLocked(uint32_t port) {
    const int bank = index_ >> kIndexBankShift;
    if (bank >= kSockets)
        return floating_.Read8("RF5C396 data register", port);
    const uint8_t sub = index_ & kIndexSubMask;
    uint8_t v = 0u;
    if (chip_.Read(bank, sub, &v))
        return v;
    if (!Exca(bank).ReadReg(sub, &v))
        emu_.Get<Fatal>().Die("[RF5C396] read of INDEX 0x%02X, a register the RF5C396 does not document", index_);
    if (sub == kRegInterfaceStatus && GpiLowLocked(bank))
        v |= kIfsGpiInverted;
    return v;
}

void RicohRf5c396::WriteDataLocked(int bank, uint8_t value, ExcaWriteEffect* effect) {
    const uint8_t sub = index_ & kIndexSubMask;
    if (RicohRf5c396ChipRegisters::Owns(sub)) {
        WriteChipRegisterLocked(bank, sub, value, effect);
        return;
    }
    if (!Exca(bank).Owns(sub))
        emu_.Get<Fatal>().Die("[RF5C396] write 0x%02X to INDEX 0x%02X, a register the RF5C396 does not document",
                              value, index_);
    if (!Exca(bank).WriteReg(sub, value, effect))
        emu_.Get<Fatal>().Die("[RF5C396] INDEX 0x%02X value 0x%02X selects a reserved power or IRQ code",
                              index_, value);
}

void RicohRf5c396::WriteChipRegisterLocked(int bank, uint8_t sub, uint8_t value, ExcaWriteEffect* effect) {
    using Regs = RicohRf5c396ChipRegisters;
    if (const char* refused = chip_.Write(bank, sub, value))
        emu_.Get<Fatal>().Die("[RF5C396] INDEX 0x%02X value 0x%02X %s", index_, value, refused);
    if (sub == Regs::kRegCardDetectControl) {
        if ((value & Regs::kCdcSoftwareCardDetect) != 0u) Exca(bank).SoftwareCardDetect();
        if (!chip_.GpiEnabled(bank)) Exca(bank).ClearGpiChange();
    }
    ApplyChipModesLocked();
    if (sub == Regs::kRegModeControl3)
        Exca(bank).SetInterfaceIsolated(chip_.InterfaceDisabled(bank), effect);
}

bool RicohRf5c396::DecodeMem(uint32_t off, CardMemHit* hit) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    for (int b = 0; b < kSockets; ++b) {
        if (Exca(b).MapMem(off, &hit->addr, &hit->attribute, &hit->writable, &hit->sixteen_bit)) {
            hit->bank = b;
            hit->addr = chip_.CardAddressHighLines(b, hit->addr);
            hit->isolated = chip_.InterfaceDisabled(b);
            return true;
        }
    }
    return false;
}

bool RicohRf5c396::DecodeIo(uint32_t off, CardIoHit* hit) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    for (int b = 0; b < kSockets; ++b) {
        if (Exca(b).MapIo(off, &hit->io, &hit->width)) {
            hit->bank = b;
            hit->isolated = chip_.InterfaceDisabled(b);
            hit->inpack_gated = chip_.InpackEnabled(b);
            return true;
        }
    }
    return false;
}

bool RicohRf5c396::Memcs16Asserted(uint32_t off) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    for (int b = 0; b < kSockets; ++b) {
        const unsigned shift = (Exca(b).WindowEnable() & kWinMemcs16FullDecode) != 0u
            ? kMemcs16FullDecode : kMemcs16UpperDecode;
        if (Exca(b).SixteenBitWindowDecodes(off, shift)) return true;
    }
    return false;
}

uint8_t RicohRf5c396::ReadCardMem8(uint32_t off) {
    CardMemHit hit;
    if (!DecodeMem(off, &hit) || hit.isolated)
        return floating_.Read8("RF5C396 ISA memory", off);
    const uint8_t card = static_cast<uint8_t>(
        ReadCardMemory(Slot(hit.bank), hit.addr, hit.attribute, hit.sixteen_bit, 1u));
    if (!hit.sixteen_bit && (off & 1u) != 0u && Memcs16Asserted(off))
        return floating_.Read8("RF5C396 high lane of an 8-bit window cycle", off);
    return card;
}

uint16_t RicohRf5c396::ReadCardMem16(uint32_t off) {
    CardMemHit hit;
    if (!DecodeMem(off, &hit) || hit.isolated)
        return floating_.Read16("RF5C396 ISA memory", off);
    if (hit.sixteen_bit)
        return static_cast<uint16_t>(ReadCardMemory(Slot(hit.bank), hit.addr, hit.attribute, true, 2u));
    if (!Memcs16Asserted(off))
        return static_cast<uint16_t>(ReadCardMemory(Slot(hit.bank), hit.addr, hit.attribute, false, 2u));
    const uint32_t low = ReadCardMemory(Slot(hit.bank), hit.addr, hit.attribute, false, 1u);
    const uint32_t high = floating_.Read8("RF5C396 high byte of an 8-bit window cycle", off + 1u);
    return static_cast<uint16_t>(low | (high << 8));
}

void RicohRf5c396::WriteCardMem8(uint32_t off, uint8_t value) {
    CardMemHit hit;
    if (!DecodeMem(off, &hit)) {
        floating_.Write("RF5C396 ISA memory", 8u, off);
        return;
    }
    if (!hit.writable || hit.isolated)
        return;
    WriteCardMemory(Slot(hit.bank), hit.addr, hit.attribute, hit.sixteen_bit, 1u, value);
}

void RicohRf5c396::WriteCardMem16(uint32_t off, uint16_t value) {
    CardMemHit hit;
    if (!DecodeMem(off, &hit)) {
        floating_.Write("RF5C396 ISA memory", 16u, off);
        return;
    }
    if (!hit.writable || hit.isolated)
        return;
    if (hit.sixteen_bit) {
        WriteCardMemory(Slot(hit.bank), hit.addr, hit.attribute, true, 2u, value);
        return;
    }
    if (!Memcs16Asserted(off)) {
        WriteCardMemory(Slot(hit.bank), hit.addr, hit.attribute, false, 2u, value);
        return;
    }
    WriteCardMemory(Slot(hit.bank), hit.addr, hit.attribute, false, 1u, value & 0xFFu);
}

uint32_t RicohRf5c396::MaskFloppyStatusBit7(int bank, uint32_t off, uint32_t value, unsigned bytes) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    for (unsigned lane = 0; lane < bytes; ++lane) {
        if (!chip_.DisablesDataBit7(bank, off + lane)) continue;
        const uint32_t bit = static_cast<uint32_t>(kDataBit7) << (8u * lane);
        const uint32_t floating = static_cast<uint32_t>(floating_.Read8("RF5C396 data bit 7", off + lane)) << (8u * lane);
        value = (value & ~bit) | (floating & bit);
    }
    return value;
}

uint8_t RicohRf5c396::ReadCardIo8(uint32_t off) {
    CardIoHit hit;
    if (!DecodeIo(off, &hit) || hit.isolated)
        return floating_.Read8("RF5C396 ISA I/O", off);
    const uint32_t v = ReadCardIo(Slot(hit.bank), hit.io, hit.width, 1u);
    if (hit.inpack_gated && !Slot(hit.bank).InputAcknowledge(hit.io))
        return floating_.Read8("RF5C396 I/O read without INPACK#", off);
    return static_cast<uint8_t>(MaskFloppyStatusBit7(hit.bank, off, v, 1u));
}

uint16_t RicohRf5c396::ReadCardIo16(uint32_t off) {
    CardIoHit hit;
    if (!DecodeIo(off, &hit) || hit.isolated)
        return floating_.Read16("RF5C396 ISA I/O", off);
    const uint32_t v = ReadCardIo(Slot(hit.bank), hit.io, hit.width, 2u);
    if (hit.inpack_gated && !Slot(hit.bank).InputAcknowledge(hit.io))
        return floating_.Read16("RF5C396 I/O read without INPACK#", off);
    return static_cast<uint16_t>(MaskFloppyStatusBit7(hit.bank, off, v, 2u));
}

void RicohRf5c396::WriteCardIo8(uint32_t off, uint8_t value) {
    CardIoHit hit;
    if (!DecodeIo(off, &hit)) {
        floating_.Write("RF5C396 ISA I/O", 8u, off);
        return;
    }
    if (hit.isolated)
        return;
    WriteCardIo(Slot(hit.bank), hit.io, hit.width, 1u, value);
}

void RicohRf5c396::WriteCardIo16(uint32_t off, uint16_t value) {
    CardIoHit hit;
    if (!DecodeIo(off, &hit)) {
        floating_.Write("RF5C396 ISA I/O", 16u, off);
        return;
    }
    if (hit.isolated)
        return;
    WriteCardIo(Slot(hit.bank), hit.io, hit.width, 2u, value);
}

bool RicohRf5c396::CardInterruptLocked(int bank) {
    Pcic82365& exca = Exca(bank);
    return chip_.IreqActiveHigh(bank) ? exca.ReadyPin() : exca.IreqPinLow();
}

void RicohRf5c396::UpdateIrqLineLocked() {
    const uint8_t wired = board_->WiredIrqSelect();
    const bool edge = !chip_.LevelInterrupts();
    bool steered = false;
    bool pending = false;
    for (int b = 0; b < kSockets; ++b) {
        Pcic82365& exca = Exca(b);
        if (chip_.CardDetectResume(b) && (exca.ActiveStatusChangeBits() & kStatusChangeCardDetect) != 0u)
            emu_.Get<Fatal>().Die("[RF5C396] socket %d card detect resume drives RI_OUT#, whose board "
                                  "connection is not modeled", b);
        const bool csc_active = !chip_.CardDetectResume(b) && exca.StatusChangeInterruptActive();
        if ((exca.InterruptAndGeneralControl() & kIntrCscToIntr) != 0u) {
            emu_.Get<Fatal>().Die("[RF5C396] socket %d status change interrupt routed to INTR#, whose board "
                                  "connection is not modeled", b);
        } else if (exca.CscIrqSelect() == wired) {
            steered = true;
            pending = pending || csc_active;
        } else if (exca.CscIrqSelect() != kNoIrqSelect && (edge || csc_active)) {
            emu_.Get<Fatal>().Die("[RF5C396] socket %d status change interrupt routed to IRQ%u, whose board "
                                  "connection is not modeled", b, exca.CscIrqSelect());
        }
        if (!exca.IoCardInterface())
            continue;
        const bool card_active = CardInterruptLocked(b);
        if (exca.CardIrqSelect() == wired) {
            steered = true;
            pending = pending || card_active;
        } else if (exca.CardIrqSelect() != kNoIrqSelect && (edge || card_active)) {
            emu_.Get<Fatal>().Die("[RF5C396] socket %d card interrupt routed to IRQ%u, whose board "
                                  "connection is not modeled", b, exca.CardIrqSelect());
        }
    }
    if (!edge) {
        board_->SetIrqLevel(pending ? false : kUndrivenIrqNetLevel);
        return;
    }
    board_->SetIrqLevel(pending || !steered);
}

void RicohRf5c396::OnCardDetectChanged(PcmciaSlot& slot) {
    sequencer_.Run([&] {
        ExcaSocketUpdates updates{};
        std::lock_guard<std::mutex> lk(state_mutex_);
        const int bank = BankOf(slot);
        Exca(bank).CardDetectChanged(slot.HasCard(), &updates[0].effect);
        TrackGpiLocked(bank);
        updates[0].slot = &slot;
        UpdateIrqLineLocked();
        return updates;
    });
}

void RicohRf5c396::OnReadyIreqLow(PcmciaSlot& slot) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    Exca(BankOf(slot)).SetReadyIreqLow(true);
    UpdateIrqLineLocked();
}

void RicohRf5c396::OnReadyIreqHigh(PcmciaSlot& slot) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    Exca(BankOf(slot)).SetReadyIreqLow(false);
    UpdateIrqLineLocked();
}

void RicohRf5c396::SaveState(StateWriter& w) {
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        w.Write("index", index_);
        chip_.SaveGlobal(w);
    }
    for (int bank = 0; bank < kSockets; ++bank) {
        w.BeginFrame(static_cast<uint32_t>(bank));
        {
            std::lock_guard<std::mutex> lk(state_mutex_);
            chip_.SaveBank(w, bank);
            Exca(bank).SaveState(w);
        }
        Slot(bank).SaveSlotState(w);
        w.EndFrame();
    }
}

void RicohRf5c396::RestoreState(StateReader& r) {
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        r.Read("index", index_);
        chip_.RestoreGlobal(r);
    }
    for (int bank = 0; bank < kSockets; ++bank) {
        if (const uint32_t frame = r.EnterFrame(); frame != static_cast<uint32_t>(bank))
            r.Reject("RF5C396 socket frame 0x%X where this build writes socket %d", frame, bank);
        {
            std::lock_guard<std::mutex> lk(state_mutex_);
            chip_.RestoreBank(r, bank);
            Exca(bank).RestoreState(r);
        }
        Slot(bank).RestoreSlotState(r);
        r.LeaveFrame();
    }
    std::lock_guard<std::mutex> lk(state_mutex_);
    ApplyChipModesLocked();
    gpi_low_[0] = GpiLowLocked(0);
    gpi_low_[1] = GpiLowLocked(1);
}

void RicohRf5c396::PostRestore() {
    sequencer_.Run([this] {
        std::lock_guard<std::mutex> lk(state_mutex_);
        return ExcaSocketUpdates{{
            {&slot0_, ExcaWriteEffect::Outputs(exca0_.OutputsEnabled())},
            {&slot1_, ExcaWriteEffect::Outputs(exca1_.OutputsEnabled())} }};
    });
    slot0_.PostRestoreSlot();
    slot1_.PostRestoreSlot();
    std::lock_guard<std::mutex> lk(state_mutex_);
    UpdateIrqLineLocked();
}

REGISTER_SERVICE(RicohRf5c396);

}
