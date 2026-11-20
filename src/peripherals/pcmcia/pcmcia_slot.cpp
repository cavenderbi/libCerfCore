#include "pcmcia_slot.h"

#include "pcmcia_card_catalog.h"
#include "pcmcia_card_identity.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/core/product_name.h>
#include <cerfcore/state/emulation_freeze.h>
#include <cerfcore/state/state_stream.h>

#include <memory>
#include <utility>

namespace cerfcore {

uint16_t PcmciaSlot::UndrivenDataWord() const {
    const uint8_t lane = host_.UndrivenDataLane();
    return static_cast<uint16_t>((lane << 8) | lane);
}

PcmciaSlot::PcmciaSlot(CerfEmulator& emu, PcmciaSlotHost& host, ObjectIdentity identity)
    : emu_(emu), host_(host), identity_(std::move(identity)),
      gap_(emu, [this](std::unique_ptr<PcmciaCard> card) { SeatHeld(std::move(card)); }) {}

void PcmciaSlot::PublishInsertedLocked() {
    std::optional<InsertedItem> inserted = gap_.HeldItem();
    if (card_) inserted = InsertedItem{card_->SaveId(), card_->DisplayName(), card_->Facts()};
    std::lock_guard<std::mutex> lk(inserted_mtx_);
    inserted_ = std::move(inserted);
}

void PcmciaSlot::Announce(bool ejected, bool inserted) {
    if (ejected) ReportEjected();
    if (inserted) ReportInserted();
}

void PcmciaSlot::PublishPinsLocked() {
    uint8_t v = (card_ ? kPinPresent : 0u) | (powered_ ? kPinPowered : 0u);
    if (card_) {
        const PcmciaVoltageSense vs = card_->VoltageSense();
        if (vs.vs1_grounded)    v |= kPinVs1Grounded;
        if (vs.vs2_grounded)    v |= kPinVs2Grounded;
        if (vs.low_voltage_key) v |= kPinLowVoltageKey;
    }
    pins_.store(v, std::memory_order_release);
}

void PcmciaSlot::SetPowered(bool on) {
    {
        std::lock_guard<std::mutex> lk(bus_mutex_);
        if (powered_ == on) return;
        powered_ = on;
        LOG(Pcmcia, "[Slot %s] Vcc %s\n", identity_.name.c_str(), on ? "on" : "off");
        PublishPinsLocked();
        if (card_) {
            if (on) card_->PowerOn();
            else    card_->PowerOff();
        }
        if (!on) {
            DriveLine(kLineBusy, false);
            DriveStatusChange(false);
        }
    }
    host_.OnCardPowerChanged(*this);
}

void PcmciaSlot::PowerCycleCard() {
    if (!HasCard()) return;
    SetPowered(false);
    SetPowered(true);
}

void PcmciaSlot::SetResetLine(bool asserted) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (reset_asserted_ == asserted) return;
    reset_asserted_ = asserted;
    if (!card_ || !powered_) return;
    if (asserted) {
        if (card_->HoldsReadyBusyInReset()) DriveLine(kLineBusy, true);
        return;
    }
    card_->SocketReset();
    DriveLine(kLineBusy, false);
}

void PcmciaSlot::DriveLine(uint8_t source, bool low) {
    const uint8_t before = low
        ? line_.fetch_or(source, std::memory_order_acq_rel)
        : line_.fetch_and(static_cast<uint8_t>(~source), std::memory_order_acq_rel);
    const uint8_t after = low ? static_cast<uint8_t>(before | source)
                              : static_cast<uint8_t>(before & ~source);
    if ((before != 0u) == (after != 0u)) return;
    if (after != 0u) host_.OnReadyIreqLow(*this);
    else             host_.OnReadyIreqHigh(*this);
}

void PcmciaSlot::SetCardEnabled(bool on) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    enabled_ = on;
}

bool PcmciaSlot::CardSelectedLocked() {
    return card_ && powered_ && enabled_;
}

uint8_t PcmciaSlot::ReadAttribute8(uint32_t offset) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return host_.UndrivenDataLane();
    return card_->ReadAttribute8(offset);
}

void PcmciaSlot::WriteAttribute8(uint32_t offset, uint8_t value) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return;
    card_->WriteAttribute8(offset, value);
}

uint16_t PcmciaSlot::ReadAttribute16(uint32_t offset) {
    return static_cast<uint16_t>((host_.UndrivenDataLane() << 8) | ReadAttribute8(offset & ~1u));
}

void PcmciaSlot::WriteAttribute16(uint32_t offset, uint16_t value) {
    WriteAttribute8(offset & ~1u, static_cast<uint8_t>(value));
}

uint8_t PcmciaSlot::ReadCommon8(uint32_t offset) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return host_.UndrivenDataLane();
    return card_->ReadCommon8(offset);
}

uint16_t PcmciaSlot::ReadCommon16(uint32_t offset) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return UndrivenDataWord();
    return card_->ReadCommon16(offset);
}

void PcmciaSlot::WriteCommon8(uint32_t offset, uint8_t value) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return;
    card_->WriteCommon8(offset, value);
}

void PcmciaSlot::WriteCommon16(uint32_t offset, uint16_t value) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return;
    card_->WriteCommon16(offset, value);
}

uint8_t PcmciaSlot::ReadIo8(uint32_t offset) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return host_.UndrivenDataLane();
    return card_->ReadIo8(offset);
}

uint16_t PcmciaSlot::ReadIo16(uint32_t offset) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return UndrivenDataWord();
    return card_->ReadIo16(offset);
}

void PcmciaSlot::WriteIo8(uint32_t offset, uint8_t value) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return;
    card_->WriteIo8(offset, value);
}

void PcmciaSlot::WriteIo16(uint32_t offset, uint16_t value) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked()) return;
    card_->WriteIo16(offset, value);
}

bool PcmciaSlot::Iois16(uint32_t offset) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked())
        return false;
    return card_->Iois16(offset);
}

bool PcmciaSlot::InputAcknowledge(uint32_t offset) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (!CardSelectedLocked())
        return false;
    return card_->InputAcknowledge(offset);
}

void PcmciaSlot::RaiseIrq() { DriveLine(kLineIreq, true); }
void PcmciaSlot::ClearIrq() { DriveLine(kLineIreq, false); }

void PcmciaSlot::DriveStatusChange(bool low) {
    if (status_change_.exchange(low, std::memory_order_acq_rel) == low) return;
    host_.OnStatusChange(*this, low);
}

ExternalDeviceClock::Timer& PcmciaSlot::CardTimer() {
    if (!card_timer_) card_timer_ = &emu_.Get<ExternalDeviceClock>().AddTimer([this] { OnCardTimer(); });
    return *card_timer_;
}

void PcmciaSlot::ArmCardTimer(int64_t delay_ns) {
    emu_.Get<ExternalDeviceClock>().Arm(CardTimer(), delay_ns);
}

void PcmciaSlot::DisarmCardTimer() { emu_.Get<ExternalDeviceClock>().Disarm(CardTimer()); }

void PcmciaSlot::OnCardTimer() {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (card_) card_->OnSlotTimer();
}

void PcmciaSlot::InsertLocked(std::unique_ptr<PcmciaCard> card) {
    card_ = std::move(card);
    ++generation_;
    PublishPinsLocked();
    card_->AttachSlot(this, generation_);
    card_->OnInserted();
    if (powered_) card_->PowerOn();
    PublishInsertedLocked();
    LOG(Pcmcia, "[Slot %s] inserted: %s\n", identity_.name.c_str(),
        card_->DisplayName().c_str());
}

void PcmciaSlot::EjectLocked() {
    LOG(Pcmcia, "[Slot %s] ejected: %s\n", identity_.name.c_str(),
        card_->DisplayName().c_str());
    if (powered_) card_->PowerOff();
    card_.reset();
    ++generation_;
    PublishPinsLocked();
    PublishInsertedLocked();
    DriveLine(kLineIreq | kLineBusy, false);
    DriveStatusChange(false);
}

void PcmciaSlot::InsertCard(std::unique_ptr<PcmciaCard> card) {
    {
        std::lock_guard<std::mutex> lk(bus_mutex_);
        if (shut_down_) return;
        if (card_) {
            LOG(Caution, "[Slot %s] InsertCard into occupied slot "
                    "(%s) - rejected\n",
                identity_.name.c_str(), card_->DisplayName().c_str());
            return;
        }
        InsertLocked(std::move(card));
    }
    host_.OnCardDetectChanged(*this);
    Announce(false, true);
}

void PcmciaSlot::SeatHeld(std::unique_ptr<PcmciaCard> card) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(bus_mutex_);
        if (shut_down_ || card_) return;
        const std::optional<InsertedItem> held = Inserted();
        InsertLocked(std::move(card));
        changed = Inserted() != held;
    }
    host_.OnCardDetectChanged(*this);
    if (changed) Changed();
}

void PcmciaSlot::OnShutdown() {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    ++generation_;
    shut_down_ = true;
    if (card_) card_->OnShutdown();
}

void PcmciaSlot::SaveSlotState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    w.Write<uint8_t>("powered", powered_ ? 1u : 0u);
    w.Write<uint8_t>("reset_line", reset_asserted_ ? 1u : 0u);
    w.Write<uint8_t>("card_busy", (line_.load(std::memory_order_acquire) & kLineBusy) != 0u ? 1u : 0u);
    w.Write<uint8_t>("status_change", status_change_.load(std::memory_order_acquire) ? 1u : 0u);
    emu_.Get<ExternalDeviceClock>().SaveTimer(CardTimer(), w, "card_timer_");
    gap_.Save(w);
    const bool has = (card_ != nullptr);
    w.Write<uint8_t>("has", has ? 1u : 0u);
    if (!has) return;
    PcmciaCardIdentity{card_->SaveId(), card_->SaveBinding()}.Save(w, "");
    w.BeginFrame(0);
    card_->SaveState(w);
    w.EndFrame();
}

void PcmciaSlot::PostRestoreSlot() {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    emu_.Get<ExternalDeviceClock>().ResumeTimer(CardTimer());
    gap_.PostRestore();
    if (card_) card_->PostRestore();
}

void PcmciaSlot::RestoreSlotState(StateReader& r) {
    bool ejected  = false;
    bool inserted = false;
    {
        std::lock_guard<std::mutex> lk(bus_mutex_);
        RestoreSlotBodyLocked(r, ejected, inserted);
    }
    Announce(ejected, inserted);
}

void PcmciaSlot::RestoreSlotBodyLocked(StateReader& r, bool& ejected, bool& inserted) {
    ++generation_;
    uint8_t powered = 0, reset_line = 0, card_busy = 0, status_change = 0, has = 0;
    r.Read("powered", powered);
    r.Read("reset_line", reset_line);
    r.Read("card_busy", card_busy);
    r.Read("status_change", status_change);
    emu_.Get<ExternalDeviceClock>().RestoreTimer(CardTimer(), r, "card_timer_");
    ejected = gap_.HeldItem().has_value();
    gap_.Restore(r);
    inserted = gap_.HeldItem().has_value();
    r.Read("has", has);
    reset_asserted_ = reset_line != 0u;
    if (!has) {
        if (card_) {
            EjectLocked();
            ejected = true;
        }
        powered_ = (powered != 0);
        PublishPinsLocked();
        PublishInsertedLocked();
        line_.store(card_busy != 0u ? kLineBusy : 0u, std::memory_order_release);
        status_change_.store(status_change != 0u, std::memory_order_release);
        return;
    }
    const PcmciaCardIdentity identity = PcmciaCardIdentity::Restore(r, "");
    if (const uint32_t frame = r.EnterFrame(); frame != 0u)
        r.Reject("PC Card body in frame 0x%X, this build writes frame 0", frame);
    if (!card_ || identity.id != card_->SaveId()) {
        auto card = emu_.Get<PcmciaCardCatalog>().TryCreate(identity.id, identity.binding);
        if (!card) r.Reject("PC Card '%s' is unknown to this build", identity.id.c_str());
        if (card_) {
            EjectLocked();
            ejected = true;
        }
        powered_ = (powered != 0);
        PublishPinsLocked();
        InsertLocked(std::move(card));
        inserted = true;
    } else if (powered_ != (powered != 0)) {
        powered_ = (powered != 0);
        PublishPinsLocked();
        if (powered_) card_->PowerOn();
        else          card_->PowerOff();
    }
    card_->RestoreState(r);
    r.LeaveFrame();
    line_.store(card_busy != 0u ? kLineBusy : 0u, std::memory_order_release);
    status_change_.store(status_change != 0u, std::memory_order_release);
    PublishInsertedLocked();
}

void PcmciaSlot::EjectCardIfResident(uint64_t card_id) {
    {
        std::lock_guard<std::mutex> lk(bus_mutex_);
        if (shut_down_ || !card_ || card_->CardId() != card_id) return;
        card_->StopHostIo();
    }
    {
        auto frozen = emu_.Get<EmulationFreeze>().WorkerSection();
        {
            std::lock_guard<std::mutex> lk(bus_mutex_);
            if (shut_down_ || !card_ || card_->CardId() != card_id) return;
            EjectLocked();
            gap_.Open();
        }
        host_.OnCardDetectChanged(*this);
    }
    Announce(true, false);
}

void PcmciaSlot::ReportCardFailure(uint64_t card_id, const char* reason, uint32_t code) {
    std::string subject;
    {
        std::lock_guard<std::mutex> lk(bus_mutex_);
        if (shut_down_ || !card_ || card_->CardId() != card_id) return;
        subject = card_->SaveBinding();
    }
    Failed(reason, code, std::move(subject));
    EjectCardIfResident(card_id);
}

std::optional<uint64_t> PcmciaSlot::StopResidentHostIo() {
    std::lock_guard<std::mutex> lk(bus_mutex_);
    if (shut_down_) return std::nullopt;
    if (card_) card_->StopHostIo();
    return generation_;
}

void PcmciaSlot::Eject() {
    bool ejected = false;
    for (;;) {
        const std::optional<uint64_t> gen = StopResidentHostIo();
        if (!gen) return;
        auto                         frozen = emu_.Get<EmulationFreeze>().WorkerSection();
        std::unique_lock<std::mutex> lk(bus_mutex_);
        if (shut_down_) return;
        if (generation_ != *gen) continue;
        bool pins = false;
        if (card_) {
            EjectLocked();
            gap_.Open();
            pins    = true;
            ejected = true;
        } else if (gap_.Drop()) {
            LOG(Pcmcia, "[Slot %s] dropped the held card\n", identity_.name.c_str());
            PublishInsertedLocked();
            ejected = true;
        }
        lk.unlock();
        if (pins) host_.OnCardDetectChanged(*this);
        break;
    }
    Announce(ejected, false);
}

void PcmciaSlot::CombinedSwap(std::unique_ptr<PcmciaCard> card) {
    bool ejected = false;
    for (;;) {
        const std::optional<uint64_t> gen = StopResidentHostIo();
        if (!gen) return;
        auto                         frozen = emu_.Get<EmulationFreeze>().WorkerSection();
        std::unique_lock<std::mutex> lk(bus_mutex_);
        if (shut_down_) return;
        if (generation_ != *gen) continue;
        bool pins = false;
        if (card_) {
            EjectLocked();
            gap_.Open();
            pins    = true;
            ejected = true;
        }
        bool              replaced = false;
        const std::string name     = card->DisplayName();
        if (gap_.Hold(card, replaced)) {
            LOG(Pcmcia, "[Slot %s] held for the swap gap: %s (replaced a held card: %d)\n",
                identity_.name.c_str(), name.c_str(), replaced ? 1 : 0);
            PublishInsertedLocked();
            ejected = ejected || replaced;
        } else {
            InsertLocked(std::move(card));
            pins = true;
        }
        lk.unlock();
        if (pins) host_.OnCardDetectChanged(*this);
        break;
    }
    Announce(ejected, true);
}

std::optional<InsertedItem> PcmciaSlot::Inserted() const {
    std::lock_guard<std::mutex> lk(inserted_mtx_);
    return inserted_;
}

std::vector<InsertableType> PcmciaSlot::Accepted() const { return emu_.Get<PcmciaCardCatalog>().Accepted(); }

void PcmciaSlot::InsertAccepted(const std::string& type_id, const std::string& command_id,
                                const std::string& argument) {
    emu_.Get<PcmciaCardCatalog>().Insert(
        *this, type_id, command_id, argument,
        PcmciaCardCatalog::CardInserter{
            [this](std::unique_ptr<PcmciaCard> card) { CombinedSwap(std::move(card)); },
            [this](const char* reason, std::string subject) { Failed(reason, 0, std::move(subject)); }});
}

}
