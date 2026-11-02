#include "pci1225_socket_function.h"

#include "pci1225_legacy_port.h"
#include "ti_cardbus_board.h"
#include "../pcmcia/pcmcia_host_cycles.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr ExcaPartRules kExcaRules = [] {
    ExcaPartRules rules{};
    rules.ready_change_any_edge = false;
    rules.latch_requires_enable = true;
    rules.status_read_masked_by_enable = true;
    rules.auto_power = false;
    rules.power_control_reserved = 0x64u;
    rules.power_reserved_field_a = 0x18u;
    rules.power_reserved_code_a = 0x08u;
    rules.power_reserved_field_b = 0x03u;
    rules.power_reserved_code_b = 0x03u;
    rules.window_enable_reserved = 0x20u;
    rules.memory_end_high_reserved = 0x30u;
    rules.io_offset_low_reserved = 0x01u;
    rules.window_page_registers = true;
    rules.socket_control_power = true;
    rules.stschg_change_any_edge = false;
    rules.battery_dead_pin = ExcaBatteryDeadPin::kBvd2Low;
    rules.battery_warning = ExcaBatteryWarning::kEnterBvd2HighBvd1Low;
    rules.outputs_require_card = false;
    rules.ring_indicate_masks_status_change = false;
    rules.irq_select_reserved = 0x0000u;
    return rules;
}();

constexpr uint8_t  kIntGenCscRoute   = 0x10u;
constexpr uint8_t  kIntGenIntSelect  = 0x0Fu;
constexpr uint8_t  kSmiIsaIrq        = 2u;

constexpr uint8_t  kGlobalCtrlIfcMode = 0x04u;

constexpr uint8_t kPwrVccField      = 0x18u;
constexpr uint8_t kPwrVppField      = 0x03u;
constexpr uint8_t kPwrVpp12         = 0x02u;
constexpr uint8_t kIfsBvd1          = 0x01u;

constexpr uint32_t kSocketBarSize    = 0x1000u;
constexpr uint32_t kExCaWindowBase   = 0x800u;
constexpr uint32_t kExCaLastOffset   = 0x44u;
constexpr uint32_t kZoomVideoCardLines = 0xFu << 22;

}

Pci1225SocketFunction::Pci1225SocketFunction(CerfEmulator& emu, uint8_t function, const char* slot_label,
                                             Pci1225SharedConfig& shared, std::mutex& mtx,
                                             ExcaSocketSequencer& sequencer)
    : emu_(emu),
      shared_(shared),
      mtx_(mtx),
      sequencer_(sequencer),
      slot_(emu, *this, slot_label),
      exca_(kExcaRules),
      config_(function, shared) {}

void Pci1225SocketFunction::Attach(TiCardbusBoard& board, Pci1225SocketFunction& peer, Pci1225LegacyPort* legacy) {
    board_ = &board;
    peer_ = &peer;
    legacy_ = legacy;
    pin_.Attach(board, peer.pin_);
}

uint8_t Pci1225SocketFunction::PciDev() const {
    return board_->PciDevice();
}

void Pci1225SocketFunction::ResetLocked() {
    config_.Reset();
    socket_regs_.Reset();
    exca_.Reset();
    chip_regs_.Reset();
    ApplyGlobalControlLocked();
    functional_level_ = false;
    cardsts_level_ = CardStsLevelLocked();
}

uint32_t Pci1225SocketFunction::ConfigRead(uint32_t reg) {
    std::lock_guard<std::mutex> lk(mtx_);
    uint32_t value = 0u;
    if (!config_.Read(reg, &value))
        emu_.Get<Fatal>().Die("PCI1225 function %u: config read at 0x%02X, outside Table 18",
                              config_.Function(), reg);
    return value;
}

void Pci1225SocketFunction::ConfigWrite(uint32_t reg, uint32_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (const char* refused = config_.Write(reg, value))
        emu_.Get<Fatal>().Die("PCI1225 function %u: config write 0x%08X at 0x%02X %s",
                              config_.Function(), value, reg, refused);
    UpdateLineLocked(false);
    peer_->UpdateLineLocked(false);
}

bool Pci1225SocketFunction::MemClaims(uint32_t addr) const {
    return SocketBarHit(addr) || CardMemHit(addr);
}

uint32_t Pci1225SocketFunction::MemRead(uint32_t addr, unsigned size) {
    if (CardMemHit(addr))
        return CardMemRead(addr, size);
    uint32_t off = 0u;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        off = addr - config_.SocketBarBase();
    }
    if (off >= kExCaWindowBase)
        return ExCaRead(off - kExCaWindowBase, size);
    return SocketRead(off, size);
}

void Pci1225SocketFunction::MemWrite(uint32_t addr, uint32_t value, unsigned size) {
    if (CardMemHit(addr)) {
        CardMemWrite(addr, value, size);
        return;
    }
    uint32_t off = 0u;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        off = addr - config_.SocketBarBase();
    }
    if (off >= kExCaWindowBase) {
        ExCaWrite(off - kExCaWindowBase, value, size);
        return;
    }
    SocketWrite(off, value, size);
}

bool Pci1225SocketFunction::LegacyClaimLocked(uint32_t pci_io) const {
    return legacy_ != nullptr && legacy_->ClaimsLocked(pci_io, config_.IoSpaceEnabled());
}

bool Pci1225SocketFunction::IoClaims(uint32_t pci_io) const {
    std::lock_guard<std::mutex> lk(mtx_);
    uint32_t card = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    return LegacyClaimLocked(pci_io) || (config_.IoSpaceEnabled() && exca_.MapIo(pci_io, &card, &width));
}

uint32_t Pci1225SocketFunction::IoRead(uint32_t pci_io, unsigned size) {
    bool legacy = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        legacy = LegacyClaimLocked(pci_io);
    }
    if (legacy)
        return legacy_->Read(pci_io, size);
    uint32_t card = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    MapClaimedIo(pci_io, &card, &width);
    RequireIoCycleOrder("I/O read", pci_io, card, width, size);
    return ReadCardIo(slot_, card, width, size);
}

void Pci1225SocketFunction::IoWrite(uint32_t pci_io, uint32_t value, unsigned size) {
    bool legacy = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        legacy = LegacyClaimLocked(pci_io);
    }
    if (legacy) {
        legacy_->Write(pci_io, value, size);
        return;
    }
    uint32_t card = 0u;
    PcmciaIoWidth width = PcmciaIoWidth::kEightBit;
    MapClaimedIo(pci_io, &card, &width);
    RequireIoCycleOrder("I/O write", pci_io, card, width, size);
    WriteCardIo(slot_, card, width, size, value);
}

void Pci1225SocketFunction::OnCardDetectChanged(PcmciaSlot&) {
    sequencer_.Run([this] {
        ExcaSocketUpdates updates{};
        std::lock_guard<std::mutex> lk(mtx_);
        if (slot_.HasCard()) socket_regs_.Interrogate();
        exca_.CardDetectChanged(slot_.HasCard(), &updates[0].effect);
        updates[0].slot = &slot_;
        socket_regs_.Latch(Pci1225SocketRegisters::kEventCardDetect);
        UpdateLineLocked(false);
        return updates;
    });
}

void Pci1225SocketFunction::OnReadyIreqLow(PcmciaSlot&) {
    SetReadyIreqLow(true);
}

void Pci1225SocketFunction::OnReadyIreqHigh(PcmciaSlot&) {
    SetReadyIreqLow(false);
}

void Pci1225SocketFunction::SaveState(StateWriter& w) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (config_.Function() == 0u)
            shared_.SaveState(w);
        config_.SaveState(w);
        socket_regs_.SaveState(w);
        pin_.SaveState(w);
        chip_regs_.SaveState(w);
        exca_.SaveState(w);
    }
    slot_.SaveSlotState(w);
}

void Pci1225SocketFunction::RestoreState(StateReader& r) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (config_.Function() == 0u)
            shared_.RestoreState(r);
        config_.RestoreState(r);
        socket_regs_.RestoreState(r);
        pin_.RestoreState(r);
        chip_regs_.RestoreState(r);
        exca_.RestoreState(r);
        functional_level_ = exca_.FunctionalInterrupt();
        cardsts_level_ = CardStsLevelLocked();
    }
    slot_.RestoreSlotState(r);
}

void Pci1225SocketFunction::PostRestore() {
    sequencer_.Run([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        ApplyGlobalControlLocked();
        return ExcaSocketUpdates{{ {&slot_, ExcaWriteEffect::Outputs(exca_.OutputsEnabled())} }};
    });
    slot_.PostRestoreSlot();
    std::lock_guard<std::mutex> lk(mtx_);
    UpdateLineLocked(true);
}

void Pci1225SocketFunction::ApplyGlobalControlLocked() {
    exca_.SetExplicitStatusClear((shared_.exca_global_control & kGlobalCtrlIfcMode) != 0u);
}

bool Pci1225SocketFunction::CardStsLevelLocked() const {
    return (exca_.InterfaceStatus() & kIfsBvd1) != 0u;
}

Pci1225CardPins Pci1225SocketFunction::CardPins() const {
    Pci1225CardPins pins{};
    pins.present = slot_.HasCard();
    pins.interface_status = exca_.InterfaceStatus();
    pins.vs1_grounded = slot_.Vs1Grounded();
    pins.vs2_grounded = slot_.Vs2Grounded();
    pins.low_voltage_key = slot_.LowVoltageKey();
    pins.powered = slot_.IsPowered();
    return pins;
}

bool Pci1225SocketFunction::SocketBarHit(uint32_t addr) const {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint32_t base = config_.SocketBarBase();
    return config_.MemorySpaceEnabled() && base && addr >= base && addr < base + kSocketBarSize;
}

bool Pci1225SocketFunction::CardMemHit(uint32_t addr) const {
    std::lock_guard<std::mutex> lk(mtx_);
    uint32_t card = 0u;
    bool attr = false;
    bool wr = false;
    bool sixteen = false;
    return config_.MemorySpaceEnabled() && exca_.MapMem(addr, &card, &attr, &wr, &sixteen);
}

void Pci1225SocketFunction::MapClaimedMem(uint32_t addr, uint32_t* card, bool* attr, bool* wr, bool* sixteen) {
    std::lock_guard<std::mutex> lk(mtx_);
    exca_.MapMem(addr, card, attr, wr, sixteen);
    if (config_.ReducedZoomVideo())
        *card &= ~kZoomVideoCardLines;
    config_.NoteSocketActivity();
    socket_regs_.NoteSocketAccess();
}

void Pci1225SocketFunction::MapClaimedIo(uint32_t pci_io, uint32_t* card, PcmciaIoWidth* width) {
    std::lock_guard<std::mutex> lk(mtx_);
    exca_.MapIo(pci_io, card, width);
    config_.NoteSocketActivity();
    socket_regs_.NoteSocketAccess();
}

uint32_t Pci1225SocketFunction::CardMemRead(uint32_t addr, unsigned size) {
    uint32_t card = 0u;
    bool attr = false;
    bool wr = false;
    bool sixteen = false;
    MapClaimedMem(addr, &card, &attr, &wr, &sixteen);
    const unsigned cycles = MemoryCardCycles(sixteen, size);
    if (cycles > 1u)
        RefuseCycleOrder("memory read", addr, size, cycles);
    return ReadCardMemory(slot_, card, attr, sixteen, size);
}

void Pci1225SocketFunction::CardMemWrite(uint32_t addr, uint32_t value, unsigned size) {
    uint32_t card = 0u;
    bool attr = false;
    bool wr = false;
    bool sixteen = false;
    MapClaimedMem(addr, &card, &attr, &wr, &sixteen);
    if (!wr)
        return;
    const unsigned cycles = MemoryCardCycles(sixteen, size);
    if (cycles > 1u)
        RefuseCycleOrder("memory write", addr, size, cycles);
    WriteCardMemory(slot_, card, attr, sixteen, size, value);
}

void Pci1225SocketFunction::RequireIoCycleOrder(const char* access, uint32_t pci_io, uint32_t card,
                                                PcmciaIoWidth width, unsigned size) {
    const unsigned cycles = IoCardCycles(slot_, card, width, size);
    if (cycles > 1u && !(size == 2u && width == PcmciaIoWidth::kCardIois16))
        RefuseCycleOrder(access, pci_io, size, cycles);
}

void Pci1225SocketFunction::RefuseCycleOrder(const char* access, uint32_t addr, unsigned size, unsigned cycles) {
    emu_.Get<Fatal>().Die("PCI1225 function %u: %u-byte %s at 0x%08X needs %u card cycles; SCPS035B gives "
                          "the cycle order only for an IOIS16#-sized I/O byte pair",
                          config_.Function(), size, access, addr, cycles);
}

void Pci1225SocketFunction::RefuseExCa(const char* access, uint32_t idx) {
    emu_.Get<Fatal>().Die("PCI1225 function %u: ExCA %s at offset 0x%02X, which SCPS035B Table 43 lists as reserved",
                          config_.Function(), access, idx);
}

uint32_t Pci1225SocketFunction::SocketRead(uint32_t off, unsigned size) {
    std::lock_guard<std::mutex> lk(mtx_);
    uint32_t value = 0u;
    if (!socket_regs_.ReadWindow(off, size, CardPins(), &value))
        emu_.Get<Fatal>().Die("PCI1225 function %u: socket register read at offset 0x%03X, "
                              "a reserved or unlisted offset of Table 57", config_.Function(), off);
    return value;
}

void Pci1225SocketFunction::SocketWrite(uint32_t off, uint32_t value, unsigned size) {
    if (Pci1225SocketRegisters::IsControl(off)) {
        uint32_t control = 0u;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            control = socket_regs_.MergedControl(off, value, size);
        }
        WriteSocketControl(control);
        return;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    if (const char* refused = socket_regs_.WriteWindow(off, value, size))
        emu_.Get<Fatal>().Die("PCI1225 function %u: socket write 0x%08X at offset 0x%03X %s",
                              config_.Function(), value, off, refused);
    UpdateLineLocked(false);
}

void Pci1225SocketFunction::WriteSocketControl(uint32_t value) {
    sequencer_.Run([&] {
        ExcaSocketUpdates updates{};
        std::lock_guard<std::mutex> lk(mtx_);
        Pci1225PowerRequest request{};
        if (!socket_regs_.WriteControl(value, &request))
            emu_.Get<Fatal>().Die("PCI1225 function %u: socket control 0x%08X requests a reserved VCC or VPP code",
                                  config_.Function(), value);
        if (request.request_changed)
            config_.NotePowerRequestChange(request.vpp12_crossed);
        ExcaWriteEffect& fx = updates[0].effect;
        exca_.RequestVccFromSocketControl(request.vcc_on, &fx);
        updates[0].slot = &slot_;
        if (fx.power_changed)
            socket_regs_.Latch(Pci1225SocketRegisters::kEventPowerCycle);
        UpdateLineLocked(false);
        return updates;
    });
}

void Pci1225SocketFunction::SetReadyIreqLow(bool low) {
    std::lock_guard<std::mutex> lk(mtx_);
    exca_.SetReadyIreqLow(low);
    UpdateLineLocked(false);
}

uint32_t Pci1225SocketFunction::ExCaRead(uint32_t idx, unsigned size) {
    uint32_t v = 0u;
    for (unsigned k = 0; k < size; ++k)
        v |= static_cast<uint32_t>(ExCaReadByte(idx + k)) << (8u * k);
    return v;
}

void Pci1225SocketFunction::ExCaWrite(uint32_t idx, uint32_t value, unsigned size) {
    for (unsigned k = 0; k < size; ++k)
        ExCaWriteByte(idx + k, static_cast<uint8_t>(value >> (8u * k)));
}

uint8_t Pci1225SocketFunction::ExCaReadByte(uint32_t idx) {
    if (idx > kExCaLastOffset)
        RefuseExCa("read", idx);
    std::lock_guard<std::mutex> lk(mtx_);
    uint8_t owned = 0u;
    if (exca_.ReadReg(static_cast<uint8_t>(idx), &owned)) {
        UpdateLineLocked(false);
        return owned;
    }
    if (!Pci1225ExcaChipRegisters::Owns(static_cast<uint8_t>(idx)))
        RefuseExCa("read", idx);
    return chip_regs_.Read(static_cast<uint8_t>(idx), slot_.Vs1Grounded(), slot_.Vs2Grounded(),
                           shared_.exca_global_control);
}

void Pci1225SocketFunction::ExCaWriteByte(uint32_t idx, uint8_t value) {
    if (idx > kExCaLastOffset)
        RefuseExCa("write", idx);
    if (exca_.Owns(static_cast<uint8_t>(idx))) {
        sequencer_.Run([&] {
            ExcaSocketUpdates updates{};
            std::lock_guard<std::mutex> lk(mtx_);
            ExcaWriteEffect& fx = updates[0].effect;
            const uint8_t power_before = exca_.PowerControl();
            if (!exca_.WriteReg(static_cast<uint8_t>(idx), value, &fx))
                emu_.Get<Fatal>().Die("PCI1225 function %u: ExCA 0x%02X value 0x%02X selects a reserved power code",
                                      config_.Function(), idx, value);
            updates[0].slot = &slot_;
            NoteExcaPowerRequestLocked(power_before, exca_.PowerControl());
            if (fx.power_changed)
                socket_regs_.Latch(Pci1225SocketRegisters::kEventPowerCycle);
            UpdateLineLocked(false);
            return updates;
        });
        return;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    if (!Pci1225ExcaChipRegisters::Owns(static_cast<uint8_t>(idx)))
        RefuseExCa("write", idx);
    const Pci1225ExcaChipWrite write = chip_regs_.Write(static_cast<uint8_t>(idx), value,
                                                        config_.IdRegistersWritable(),
                                                        &shared_.exca_global_control);
    if (write.refused)
        emu_.Get<Fatal>().Die("PCI1225 function %u: ExCA 0x%02X value 0x%02X %s",
                              config_.Function(), idx, value, write.refused);
    if (write.software_csc)
        exca_.SoftwareCardDetect();
    if (write.global_changed) {
        ApplyGlobalControlLocked();
        peer_->ApplyGlobalControlLocked();
    }
    UpdateLineLocked(false);
}

void Pci1225SocketFunction::NoteExcaPowerRequestLocked(uint8_t before, uint8_t now) {
    if (((before ^ now) & (kPwrVccField | kPwrVppField)) == 0u)
        return;
    config_.NotePowerRequestChange(((before & kPwrVppField) == kPwrVpp12) != ((now & kPwrVppField) == kPwrVpp12));
}

bool Pci1225SocketFunction::CscRoutedToPciLocked() const {
    if (config_.CscToPciOnZeroCscSelect())
        return exca_.CscIrqSelect() == 0u;
    const uint8_t ctrl = exca_.InterruptAndGeneralControl();
    return (ctrl & kIntGenCscRoute) != 0u || (ctrl & kIntGenIntSelect) == 0u;
}

void Pci1225SocketFunction::UpdateLineLocked(bool force) {
    const bool functional = exca_.FunctionalInterrupt();
    if (functional && !functional_level_)
        config_.SetInterruptFlag();
    functional_level_ = functional;
    const bool cardsts = CardStsLevelLocked();
    if (cardsts != cardsts_level_)
        socket_regs_.Latch(Pci1225SocketRegisters::kEventCstsChange);
    cardsts_level_ = cardsts;
    const bool smi = config_.SmiPending();
    const bool csc = socket_regs_.EventPending() || exca_.StatusChangeInterruptActive() ||
                     (smi && config_.SmiToCsc());
    const bool csc_to_pci = CscRoutedToPciLocked();
    const bool pci_interrupt = (functional && !config_.FunctionalInterruptsViaExca()) || (csc && csc_to_pci);
    uint16_t isa_irqs = 0u;
    if (functional && config_.FunctionalInterruptsViaExca())
        isa_irqs = static_cast<uint16_t>(isa_irqs | Pci1225InterruptTerminals::IsaIrqBit(exca_.CardIrqSelect()));
    if (csc && !csc_to_pci)
        isa_irqs = static_cast<uint16_t>(isa_irqs | Pci1225InterruptTerminals::IsaIrqBit(exca_.CscIrqSelect()));
    if (smi && !config_.SmiToCsc())
        isa_irqs = static_cast<uint16_t>(isa_irqs | Pci1225InterruptTerminals::IsaIrqBit(kSmiIsaIrq));
    const Pci1225InterruptTerminals& out = config_.Terminals();
    board_->SetIsaIrqLevels(config_.Function(), out.IsaIrqsSerialized(), out.DrivenParallelIsaIrqs(),
                            out.SignaledIsaIrqs(isa_irqs));
    pin_.Drive(pci_interrupt && out.PciInterruptSignaled(), out.SignaledSlot(), out.PciInterruptsSerialized(), force);
}

}
