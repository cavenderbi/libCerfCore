#pragma once

#include "../../execution/guest_cycle_clock.h"
#include <cerfcore/peripherals/peripheral_base.h>
#include "vr41xx_kiu_key_matrix.h"
#include "vr41xx_kiu_sequencer.h"
#include "vr41xx_rtcx_ticks.h"
#include <cerfcore/core/callback_list.h>

#include <cstdint>
#include <functional>
#include <mutex>

namespace cerfengine {
class HostRequestChannel;
}

namespace cerfcore {

class Vr41xxKiuKeyPins;

struct Vr41xxKiuModel {
    uint32_t stprep_zero_count               = 0u;
    bool     scanstart_auto_clear            = false;
    bool     keyen_scanline_interlock        = false;
    bool     keyen_stop_deferred             = false;
    bool     scanstart_while_running_defined = false;
    bool     scanstp_completes_when_idle     = false;
    bool     gpen_retained_on_other_reset    = false;
    bool     gpen_survives_kiurst            = false;
    bool     scanline_none_drives_all        = false;
    bool     scan_pins_drive_piod            = false;
    uint32_t count_bias                      = 0u;
    bool     zero_count_rfu                  = false;
};

class Vr41xxKiu : public Peripheral, private Vr41xxKiuSequencerHost {
public:
    using Peripheral::Peripheral;

    void OnReady() override;

    uint32_t MmioBase() const override { return 0x0B000180u; }
    uint32_t MmioSize() const override { return 0x20u; }

    uint8_t  ReadByte (uint32_t addr) override { HaltUnsupportedAccess("KIU ReadByte", addr, 0); }
    uint32_t ReadWord (uint32_t addr) override { HaltUnsupportedAccess("KIU ReadWord", addr, 0); }
    void WriteByte(uint32_t addr, uint8_t  v) override { HaltUnsupportedAccess("KIU WriteByte", addr, v); }
    void WriteWord(uint32_t addr, uint32_t v) override { HaltUnsupportedAccess("KIU WriteWord", addr, v); }

    void SetKeyState(uint8_t matrix_index, bool pressed);

    void RegisterKportListener(std::function<void(uint8_t kport)> fn);

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

protected:
    virtual const Vr41xxKiuModel& Model() const = 0;

    virtual void     AttachFrontEnd() = 0;
    virtual GuestCycleClock::Rate ScanTickRate() = 0;
    virtual uint16_t PiodLocked() const = 0;
    virtual void     ScanStoredLocked() = 0;
    virtual void     ResetFrontEndLocked() = 0;
    virtual void     PinsChangedLocked() {}
    virtual void     FlushPinsChange() {}
    virtual void     SaveFrontEnd(StateWriter&) {}
    virtual void     RestoreFrontEnd(StateReader&) {}

    void BeginReadLocked(uint32_t addr);
    bool BeginWriteLocked(uint32_t addr, uint16_t value, bool reset_register);
    void WriteScanRepLocked(uint32_t addr, uint16_t written, uint16_t fields, uint16_t field_mask);
    void ClearCausesLocked(uint16_t bits);
    void KiuResetLocked();
    void WriteWkiLocked(uint16_t value);
    void WriteWksLocked(uint16_t value, bool scan_running);
    void PublishCausesLocked();
    void UpdateLocked();
    void EnterUndefinedLocked();
    void AdvancePhaseLocked(uint16_t piod);
    void ArmEventLocked();
    bool TimedLocked() const;

    mutable std::mutex mtx_;

    Vr41xxKiuKeyMatrix key_matrix_;
    Vr41xxKiuSequencer seq_{*this};
    uint16_t causes_    = 0;
    uint16_t wintvl_    = 0;
    uint16_t wks_       = 0x7FFF;
    uint16_t scanline_  = 0;
    uint16_t gpen_      = 0;
    bool     data_unread_ = false;
    bool     undefined_ = false;
    bool     hibernating_ = false;

    cerfengine::HostRequestChannel* host_requests_ = nullptr;

private:
    bool     scan_in_flight_ = false;
    uint32_t next_slot_ = 0;
    uint16_t scan_wks_ = 0x7FFF;
    bool     scan_key_data_ = false;
    uint8_t  kport_ = 0;
    CallbackList<uint8_t> kport_listeners_;
    uint64_t phase_end_ = 0;
    uint32_t scanrep_addr_    = 0x0B000180u;
    uint16_t scanrep_written_ = 0u;

    GuestCycleClock*        clock_ = nullptr;
    GuestCycleClock::Event* event_ = nullptr;
    Vr41xxKiuKeyPins*       key_pins_ = nullptr;
    Vr41xxRtcxTicks         rtcx_{emu_, Vr41xxRtcxDomain::Peripheral};

    bool SequencerFrozen() const override;
    bool SequencerTouch() override;
    void SequencerTouchPulse() override;
    bool SequencerScanLinesNone() const override;
    void SequencerScanEntered() override;
    void SequencerScanDataStored() override;
    [[noreturn]] void SequencerHalt(const char* op, uint16_t scanrep) override;

    bool TouchLocked() const;
    void LatchDueSlotsLocked(uint64_t now, uint16_t piod);
    uint8_t DrivenKportLocked();
    void NotifyKportLocked(uint8_t kport);
    void PublishKportLocked(bool force);
    void PulseKportLocked(uint16_t lines, uint16_t piod);
    void ArmScanLocked(uint64_t start);
    void AdvancePhaseLocked();
    void ApplyHostKeysLocked();
    uint32_t ScanPeriodTicksLocked();
    void ApplyResetLocked();
    void RequireTclockIfTimedLocked(const char* operation);
};

}
