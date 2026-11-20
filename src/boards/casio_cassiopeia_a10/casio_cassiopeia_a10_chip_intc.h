#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/core/callback_list.h>

#include <cstdint>
#include <mutex>

namespace cerfcore {

class Sh77xxIntc;

enum class CasioCassiopeiaA10ChipInput : uint8_t {
    kKeyMatrix,
    kPen,
    kAudioSwitched,
    kAudioEnded,
    kOutsideKey,
    kPcCardEvents,
    kSerialLineChange,
    kPinBankSource2,
    kPinBankSource3,
    kPinBankSource5,
    kPinBankSource7,
    kPinBankSource8,
    kPinBankSource9,
    kPinBankSource10,
    kPinBankSource14,
    kCfCardChange,
    kSlotInputChange,
    kCount,
};

class CasioCassiopeiaA10ChipIntc : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

    void     SetInputLevel(CasioCassiopeiaA10ChipInput input, bool asserted);
    uint16_t SourceLevels();
    void     AddRisingEdgeListener(CallbackList<uint16_t>::Callback listener);

private:
    uint16_t SourceLevelsLocked() const;
    uint16_t RequestsLocked() const;
    uint16_t CodeLocked() const;
    void     DriveIrlLocked();
    void     Reset();

    std::mutex             mtx_;
    CallbackList<uint16_t> rising_listeners_;
    Sh77xxIntc*            intc_       = nullptr;
    uint32_t               input_lines_ = 0u;
    uint16_t               latch_mode_ = 0u;
    uint16_t               latched_    = 0u;
    uint16_t               mask_       = 0u;
    uint16_t               driven_irl_ = 0u;
};

}
