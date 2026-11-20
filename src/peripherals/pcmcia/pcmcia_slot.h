#pragma once

#include "pcmcia_card.h"
#include "pcmcia_swap_gap.h"
#include "../external_device_clock.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace cerfcore {

class CerfEmulator;
class PcmciaSlot;

class PcmciaSlotHost {
public:
    virtual ~PcmciaSlotHost() = default;
    virtual void OnCardDetectChanged(PcmciaSlot& slot) = 0;
    virtual void OnReadyIreqLow     (PcmciaSlot& slot) = 0;
    virtual void OnReadyIreqHigh    (PcmciaSlot& slot) = 0;
    virtual void OnStatusChange     (PcmciaSlot& slot, bool low) = 0;
    virtual void OnCardPowerChanged (PcmciaSlot&) {}
    virtual uint8_t UndrivenDataLane() const = 0;

    static constexpr uint8_t kUnstatedUndrivenDataLane = 0xFFu;
};

class PcmciaSlot : public MachineSocket {
public:
    PcmciaSlot(CerfEmulator& emu, PcmciaSlotHost& host, ObjectIdentity identity);

    void SetIdentity(ObjectIdentity identity) { identity_ = std::move(identity); }

    bool HasCard()   const { return (pins_.load(std::memory_order_acquire) & kPinPresent) != 0u; }
    bool IsPowered() const {
        constexpr uint8_t live = kPinPresent | kPinPowered;
        return (pins_.load(std::memory_order_acquire) & live) == live;
    }
    bool Vs1Grounded()   const { return (pins_.load(std::memory_order_acquire) & kPinVs1Grounded) != 0u; }
    bool Vs2Grounded()   const { return (pins_.load(std::memory_order_acquire) & kPinVs2Grounded) != 0u; }
    bool LowVoltageKey() const { return (pins_.load(std::memory_order_acquire) & kPinLowVoltageKey) != 0u; }
    void SetPowered(bool on);
    void PowerCycleCard();
    void SetCardEnabled(bool on);
    void SetResetLine(bool asserted);

    uint8_t  ReadAttribute8 (uint32_t offset);
    void     WriteAttribute8(uint32_t offset, uint8_t value);
    uint16_t ReadAttribute16 (uint32_t offset);
    void     WriteAttribute16(uint32_t offset, uint16_t value);
    uint8_t  ReadCommon8  (uint32_t offset);
    uint16_t ReadCommon16 (uint32_t offset);
    void     WriteCommon8 (uint32_t offset, uint8_t  value);
    void     WriteCommon16(uint32_t offset, uint16_t value);
    uint8_t  ReadIo8  (uint32_t offset);
    uint16_t ReadIo16 (uint32_t offset);
    void     WriteIo8 (uint32_t offset, uint8_t  value);
    void     WriteIo16(uint32_t offset, uint16_t value);
    bool     Iois16   (uint32_t offset);
    bool     InputAcknowledge(uint32_t offset);
    uint8_t  UndrivenDataLane() const { return host_.UndrivenDataLane(); }
    uint16_t UndrivenDataWord() const;

    /* Card-side IRQ line, callable from any card thread; no bus lock. */
    void RaiseIrq();
    void ClearIrq();
    void DriveStatusChange(bool low);
    bool StatusChangeLow() const { return status_change_.load(std::memory_order_acquire); }

    void ArmCardTimer(int64_t delay_ns);
    void DisarmCardTimer();

    void InsertCard(std::unique_ptr<PcmciaCard> card);

    void EjectCardIfResident(uint64_t card_id);
    void ReportCardFailure(uint64_t card_id, const char* reason, uint32_t code);

    /* Forward the CerfEmulator quiesce phase to the resident card so it can
       detach from peers before any service is destroyed. Owning controllers
       call this from their Service::OnShutdown. */
    void OnShutdown();

    void SaveSlotState(StateWriter& w);
    void RestoreSlotState(StateReader& r);
    void PostRestoreSlot();

    std::string       ObjectId() const override { return identity_.id; }
    std::string       ObjectName() const override { return identity_.name; }
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kPcCardSocket; }

    std::optional<InsertedItem>  Inserted() const override;
    std::vector<InsertableType> Accepted() const override;
    void Eject() override;

protected:
    void InsertAccepted(const std::string& type_id, const std::string& command_id,
                        const std::string& argument) override;

private:
    static constexpr uint8_t kPinPresent = 0x01u;
    static constexpr uint8_t kPinPowered = 0x02u;
    static constexpr uint8_t kPinVs1Grounded   = 0x04u;
    static constexpr uint8_t kPinVs2Grounded   = 0x08u;
    static constexpr uint8_t kPinLowVoltageKey = 0x10u;
    static constexpr uint8_t kLineIreq = 0x01u;
    static constexpr uint8_t kLineBusy = 0x02u;

    void PublishPinsLocked();
    void PublishInsertedLocked();
    void Announce(bool ejected, bool inserted);
    void RestoreSlotBodyLocked(StateReader& r, bool& ejected, bool& inserted);
    void DriveLine(uint8_t source, bool low);
    bool CardSelectedLocked();

    void InsertLocked(std::unique_ptr<PcmciaCard> card);
    void EjectLocked();
    void SeatHeld(std::unique_ptr<PcmciaCard> card);
    void OnCardTimer();
    ExternalDeviceClock::Timer& CardTimer();

    std::optional<uint64_t> StopResidentHostIo();
    void                    CombinedSwap(std::unique_ptr<PcmciaCard> card);

    CerfEmulator&   emu_;
    PcmciaSlotHost& host_;
    ObjectIdentity  identity_;

    mutable std::mutex          bus_mutex_;
    std::unique_ptr<PcmciaCard> card_;
    bool                        powered_ = false;
    bool                        enabled_ = true;
    bool                        reset_asserted_ = false;
    bool                        shut_down_      = false;
    uint64_t                    generation_ = 0;
    std::atomic<uint8_t>        pins_{0};
    std::atomic<uint8_t>        line_{0};
    std::atomic<bool>           status_change_{false};

    ExternalDeviceClock::Timer* card_timer_ = nullptr;

    mutable std::mutex          inserted_mtx_;
    std::optional<InsertedItem> inserted_;

    PcmciaSwapGap gap_;
};

}
