#include "vr41xx_kiu.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../execution/engine_bridge.h"
#include <cerfengine/host_request_channel.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/state/state_stream.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include "vr41xx_cmu.h"
#include "vr41xx_icu.h"
#include "vr41xx_kiu_key_pins.h"
#include "vr41xx_kiu_regs.h"

#include <cstdint>

namespace cerfcore {

using namespace cerf_vr41xx_kiu_regs;

void Vr41xxKiu::OnReady() {
    clock_ = &emu_.Get<GuestCycleClock>();
    event_ = clock_->Add([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        UpdateLocked();
    });
    rtcx_.Attach([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        UpdateLocked();
    });
    clock_->RegisterRateListener([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        rtcx_.Rescale();
        ArmEventLocked();
    });
    key_pins_ = &emu_.Get<Vr41xxKiuKeyPins>();
    key_matrix_.Configure(Model().scanline_none_drives_all, Model().scan_pins_drive_piod);
    seq_.Configure(Model().stprep_zero_count, Model().scanstart_auto_clear,
                   Model().keyen_scanline_interlock, Model().keyen_stop_deferred,
                   Model().scanstart_while_running_defined, Model().scanstp_completes_when_idle);
    host_requests_ = &emu_.Get<EngineBridge>().Engine().Get<cerfengine::HostRequestChannel>();
    AttachFrontEnd();
    const GuestCycleClock::Rate tick = ScanTickRate();
    rtcx_.SetOscRate(tick.num, tick.den);
    host_requests_->RegisterListener([this] {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            ApplyHostKeysLocked();
        }
        FlushPinsChange();
    });
    emu_.Get<Vr41xxCmu>().RegisterClockListener(kCmuMskKiu, [this](bool supplied_after) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (supplied_after) return;
        UpdateLocked();
        if (!undefined_ && TimedLocked()) EnterUndefinedLocked();
    });
    auto& sleep = emu_.Get<GuestDeepSleep>();
    sleep.RegisterSleepEntryListener([this] {
        std::lock_guard<std::mutex> lk(mtx_);
        hibernating_ = true;
        PublishKportLocked(false);
        PinsChangedLocked();
    });
    sleep.RegisterParkClock([this] {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            ApplyHostKeysLocked();
        }
        FlushPinsChange();
    });
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (kind == ResetLineKind::Rtc || !Model().gpen_retained_on_other_reset) gpen_ = 0;
        hibernating_ = false;
        ApplyResetLocked();
        UpdateLocked();
    });
}

void Vr41xxKiu::ApplyResetLocked() {
    seq_.Reset();
    causes_  = 0;
    wintvl_  = 0;
    wks_     = kWksReset;
    scanline_ = 0;
    data_unread_ = false;
    scan_in_flight_ = false;
    next_slot_ = 0;
    scan_key_data_ = false;
    undefined_ = false;
    key_matrix_.ClearData();
    ResetFrontEndLocked();
    PublishCausesLocked();
}

bool Vr41xxKiu::TimedLocked() const {
    return seq_.State() == kSStatScanning || seq_.State() == kSStatInterval;
}

bool Vr41xxKiu::TouchLocked() const {
    return key_matrix_.Kport(scanline_, gpen_, PiodLocked(), key_matrix_.ScanPins(scanline_)) != 0u;
}

void Vr41xxKiu::LatchDueSlotsLocked(uint64_t now, uint16_t piod) {
    if (seq_.State() != kSStatScanning || !scan_in_flight_) return;
    const uint32_t bias   = Model().count_bias;
    const uint64_t start  = phase_end_ - ScanPeriodTicks(scan_wks_, ScanLineCount(scanline_), bias);
    const uint64_t slot   = SlotTicks(scan_wks_, bias);
    const uint64_t sample = SampleTicks(scan_wks_, bias);
    const uint32_t lines  = ScanLineCount(scanline_);
    while (next_slot_ < lines && start + next_slot_ * slot + sample <= now) {
        if (key_matrix_.LatchLine(next_slot_, scanline_, gpen_, piod)) scan_key_data_ = true;
        PulseKportLocked(static_cast<uint16_t>(1u << next_slot_), piod);
        ++next_slot_;
    }
}

void Vr41xxKiu::EnterUndefinedLocked() {
    undefined_ = true;
    clock_->Disarm(event_);
    PublishCausesLocked();
    PublishKportLocked(false);
    PinsChangedLocked();
}

void Vr41xxKiu::RegisterKportListener(std::function<void(uint8_t kport)> fn) {
    std::lock_guard<std::mutex> lk(mtx_);
    kport_listeners_.Add(std::move(fn));
    PublishKportLocked(true);
}

uint8_t Vr41xxKiu::DrivenKportLocked() {
    const uint16_t piod    = PiodLocked();
    const uint8_t  outputs = key_matrix_.Kport(scanline_, gpen_, piod, 0);
    if (hibernating_) return outputs;
    const uint16_t scan = key_matrix_.ScanPins(scanline_);
    if (undefined_) {
        if (key_matrix_.Kport(scanline_, gpen_, piod, scan) != outputs)
            emu_.Get<Fatal>().Die("KIU KPORT level with a key held on a KSCAN line while the sequencer "
                                  "state is undefined (scan lines 0x%04X)", scan);
        return outputs;
    }
    if (seq_.State() != kSStatWaitKeyIn) return outputs;
    return key_matrix_.Kport(scanline_, gpen_, piod, scan);
}

void Vr41xxKiu::NotifyKportLocked(uint8_t kport) {
    kport_listeners_.Call(kport);
}

void Vr41xxKiu::PublishKportLocked(bool force) {
    if (kport_listeners_.Empty()) return;
    const uint8_t kport = DrivenKportLocked();
    if (kport == kport_ && !force) return;
    kport_ = kport;
    NotifyKportLocked(kport);
}

void Vr41xxKiu::PulseKportLocked(uint16_t lines, uint16_t piod) {
    if (kport_listeners_.Empty()) return;
    const uint8_t base  = key_matrix_.Kport(scanline_, gpen_, piod, 0);
    const uint8_t pulse = key_matrix_.Kport(scanline_, gpen_, piod, lines);
    if (base != kport_) {
        kport_ = base;
        NotifyKportLocked(base);
    }
    if (pulse == base) return;
    NotifyKportLocked(pulse);
    NotifyKportLocked(base);
}

uint32_t Vr41xxKiu::ScanPeriodTicksLocked() {
    return ScanPeriodTicks(wks_, ScanLineCount(scanline_), Model().count_bias);
}

bool Vr41xxKiu::SequencerFrozen() const { return undefined_; }

bool Vr41xxKiu::SequencerTouch() { return TouchLocked(); }

void Vr41xxKiu::SequencerTouchPulse() {
    PulseKportLocked(key_matrix_.ScanPins(scanline_), PiodLocked());
}

bool Vr41xxKiu::SequencerScanLinesNone() const {
    return (scanline_ & kScanLineMask) == kScanLineNone;
}

void Vr41xxKiu::SequencerScanEntered() {
    scan_in_flight_ = false;
    causes_ |= kScanInt;
}

void Vr41xxKiu::SequencerScanDataStored() { ScanStoredLocked(); }

void Vr41xxKiu::SequencerHalt(const char* op, uint16_t scanrep) {
    emu_.Get<Fatal>().Die("%s: write 0x%04X at 0x%08X, sequencer fields 0x%04X", op, scanrep_written_,
                          scanrep_addr_, scanrep);
}

void Vr41xxKiu::PublishCausesLocked() {
    auto& icu = emu_.Get<Vr41xxIcu>();
    icu.SetKiuSource(causes_);
    icu.SetKiuUndefined(undefined_);
}

void Vr41xxKiu::RequireTclockIfTimedLocked(const char* operation) {
    if (TimedLocked()) emu_.Get<Vr41xxCmu>().RequireClock(kCmuMskKiu, operation);
}

void Vr41xxKiu::BeginReadLocked(uint32_t addr) {
    if (undefined_) HaltUnsupportedAccess("KIU read while the sequencer state is undefined", addr, 0);
    UpdateLocked();
}

bool Vr41xxKiu::BeginWriteLocked(uint32_t addr, uint16_t value, bool reset_register) {
    if (undefined_ && !reset_register)
        HaltUnsupportedAccess("KIU write while the sequencer state is undefined", addr, value);
    RequireTclockIfTimedLocked("KIU register write");
    AdvancePhaseLocked();
    return seq_.State() == kSStatScanning && scan_in_flight_;
}

void Vr41xxKiu::WriteScanRepLocked(uint32_t addr, uint16_t written, uint16_t fields, uint16_t field_mask) {
    scanrep_addr_    = addr;
    scanrep_written_ = written;
    seq_.WriteScanRep(fields, field_mask);
    PublishCausesLocked();
    UpdateLocked();
}

void Vr41xxKiu::ClearCausesLocked(uint16_t bits) {
    causes_ &= static_cast<uint16_t>(~bits);
    PublishCausesLocked();
}

void Vr41xxKiu::KiuResetLocked() {
    if (!Model().gpen_survives_kiurst) gpen_ = 0;
    ApplyResetLocked();
    UpdateLocked();
}

void Vr41xxKiu::WriteWkiLocked(uint16_t value) {
    const bool reconfigures = seq_.State() == kSStatInterval && value != wintvl_;
    wintvl_ = value;
    if (reconfigures) EnterUndefinedLocked();
    else              UpdateLocked();
}

void Vr41xxKiu::WriteWksLocked(uint16_t value, bool scan_running) {
    const bool reconfigures = scan_running && value != scan_wks_ && value != 0u;
    wks_ = value;
    if (reconfigures) EnterUndefinedLocked();
    else              ArmEventLocked();
}

void Vr41xxKiu::SetKeyState(uint8_t matrix_index, bool pressed) {
    key_pins_->Set(matrix_index, pressed);
}

void Vr41xxKiu::ApplyHostKeysLocked() {
    uint16_t pins[Vr41xxKiuKeyPins::kRegs];
    if (!key_pins_->Take(pins)) return;
    if (hibernating_) {
        key_matrix_.SetHeld(pins);
        PublishKportLocked(false);
        return;
    }
    AdvancePhaseLocked();
    key_matrix_.SetHeld(pins);
    seq_.ReevaluateWaitKeyIn();
    PublishCausesLocked();
    UpdateLocked();
}

void Vr41xxKiu::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(mtx_);
    key_matrix_.SaveData(w);
    seq_.Save(w);
    w.Write("causes", causes_);
    w.Write("wintvl", wintvl_);
    w.Write("wks", wks_);
    w.Write("scanline", scanline_);
    w.Write("gpen", gpen_);
    w.Write("data_unread", data_unread_);
    w.Write("scan_in_flight", scan_in_flight_);
    w.Write<int64_t>("phase_rel", TimedLocked() ? static_cast<int64_t>(phase_end_ - rtcx_.Now()) : 0);
    w.Write("next_slot", next_slot_);
    w.Write("scan_wks", scan_wks_);
    w.Write("scan_key_data", scan_key_data_);
    w.Write("undefined", undefined_);
    w.Write("hibernating", hibernating_);
    SaveFrontEnd(w);
}

void Vr41xxKiu::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(mtx_);
    key_matrix_.RestoreData(r);
    seq_.Restore(r);
    r.Read("causes", causes_);
    r.Read("wintvl", wintvl_);
    r.Read("wks", wks_);
    r.Read("scanline", scanline_);
    r.Read("gpen", gpen_);
    r.Read("data_unread", data_unread_);
    r.Read("scan_in_flight", scan_in_flight_);
    int64_t phase_rel = 0;
    r.Read("phase_rel", phase_rel);
    r.Read("next_slot", next_slot_);
    r.Read("scan_wks", scan_wks_);
    r.Read("scan_key_data", scan_key_data_);
    r.Read("undefined", undefined_);
    r.Read("hibernating", hibernating_);
    RestoreFrontEnd(r);
    rtcx_.Rebase();
    phase_end_ = rtcx_.Now() + static_cast<uint64_t>(phase_rel);
    key_matrix_.ClearHeld();
    key_pins_->Clear();
}

void Vr41xxKiu::PostRestore() {
    std::lock_guard<std::mutex> lk(mtx_);
    PublishCausesLocked();
    UpdateLocked();
    PublishKportLocked(true);
}

void Vr41xxKiu::ArmScanLocked(uint64_t start) {
    emu_.Get<Vr41xxCmu>().RequireClock(kCmuMskKiu, "KIU key scan");
    if (Model().zero_count_rfu) {
        if (T1Count(wks_) == 0u || T2Count(wks_) == 0u || T3Count(wks_) == 0u)
            emu_.Get<Fatal>().Die("KIU scan armed with an RFU 00000 KIUWKS count (0x%04X)", wks_);
    } else if (T1Count(wks_) + T2Count(wks_) == 0u) {
        emu_.Get<Fatal>().Die("KIU scan armed with KIUWKS T1COUNT + T2COUNT of 0 (0x%04X)", wks_);
    }
    if (data_unread_) causes_ |= kKDatLost;
    PublishCausesLocked();
    scan_in_flight_ = true;
    next_slot_ = 0;
    scan_key_data_ = false;
    scan_wks_ = wks_;
    seq_.EnterScanning();
    phase_end_ = start + ScanPeriodTicksLocked();
}

void Vr41xxKiu::AdvancePhaseLocked() { AdvancePhaseLocked(PiodLocked()); }

void Vr41xxKiu::AdvancePhaseLocked(uint16_t piod) {
    if (undefined_) return;
    const uint64_t now = rtcx_.Now();
    uint64_t at = now;
    for (;;) {
        if (seq_.State() == kSStatScanning && !scan_in_flight_) ArmScanLocked(at);
        if (!TimedLocked()) return;
        if (now < phase_end_) {
            LatchDueSlotsLocked(now, piod);
            return;
        }
        at = phase_end_;
        if (seq_.State() == kSStatScanning) {
            LatchDueSlotsLocked(at, piod);
            scan_in_flight_ = false;
            seq_.CompleteScan(scan_key_data_);
            PublishCausesLocked();
            if (seq_.State() == kSStatInterval) phase_end_ = at + wintvl_;
        } else {
            ArmScanLocked(at);
        }
    }
}

void Vr41xxKiu::ArmEventLocked() {
    if (undefined_) {
        clock_->Disarm(event_);
        return;
    }
    const uint16_t state = seq_.State();
    if (state == kSStatScanning || (state == kSStatInterval && data_unread_)) {
        rtcx_.ArmAt(event_, phase_end_);
        return;
    }
    if (state != kSStatInterval) {
        clock_->Disarm(event_);
        return;
    }
    rtcx_.ArmAt(event_, phase_end_ + ScanPeriodTicksLocked());
}

void Vr41xxKiu::UpdateLocked() {
    AdvancePhaseLocked();
    ArmEventLocked();
    PublishKportLocked(false);
    PinsChangedLocked();
}

}
