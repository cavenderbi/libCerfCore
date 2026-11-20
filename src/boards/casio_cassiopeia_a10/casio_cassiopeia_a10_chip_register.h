#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "casio_cassiopeia_a10_family_id.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <utility>

namespace cerfcore {

template <uint32_t kAddress>
class CasioCassiopeiaA10ChipWord : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override { emu_.Get<PeripheralDispatcher>().Register(this); }

    uint32_t MmioBase() const override { return kAddress; }
    uint32_t MmioSize() const override { return 2u; }
};

template <uint32_t kAddress, uint16_t kInitial>
class CasioCassiopeiaA10ChipRegisterBase : public CasioCassiopeiaA10ChipWord<kAddress> {
public:
    using CasioCassiopeiaA10ChipWord<kAddress>::CasioCassiopeiaA10ChipWord;

    void OnReady() override {
        CasioCassiopeiaA10ChipWord<kAddress>::OnReady();
        GuestCpuReset& reset = this->emu_.template Get<GuestCpuReset>();
        reset.RegisterResetListener([this](ResetLineKind) { Store(kInitial); });
        reset.RegisterResetReleaseListener([this] { change_listeners_.Call(); });
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        if (!Accepts(value)) this->HaltUnsupportedAccess("WriteHalf", addr, value);
        Apply(value);
    }

    uint16_t Value() const { return value_.load(std::memory_order_acquire); }

    void AddChangeListener(CallbackList<>::Callback listener) { change_listeners_.Add(std::move(listener)); }

    void SaveState(StateWriter& w) override { w.Write("value", Value()); }

    void RestoreState(StateReader& r) override {
        uint16_t value = 0u;
        r.Read("value", value);
        Store(value);
    }

protected:
    virtual bool Accepts(uint16_t value) const = 0;

    virtual void Apply(uint16_t value) {
        Store(value);
        change_listeners_.Call();
    }

    void Store(uint16_t value) { value_.store(value, std::memory_order_release); }

    void SetBits(uint16_t bits) {
        value_.fetch_or(bits, std::memory_order_acq_rel);
        change_listeners_.Call();
    }

    void ClearBits(uint16_t bits) {
        value_.fetch_and(static_cast<uint16_t>(~bits), std::memory_order_acq_rel);
        change_listeners_.Call();
    }

private:
    std::atomic<uint16_t> value_{kInitial};
    CallbackList<>        change_listeners_;
};

template <uint32_t kAddress, uint16_t kAccepted>
class CasioCassiopeiaA10ChipRegister : public CasioCassiopeiaA10ChipRegisterBase<kAddress, kAccepted> {
public:
    using CasioCassiopeiaA10ChipRegisterBase<kAddress, kAccepted>::CasioCassiopeiaA10ChipRegisterBase;

protected:
    bool Accepts(uint16_t value) const override { return value == kAccepted; }
};

template <uint32_t kAddress, uint16_t kInitial, const auto& kValues>
class CasioCassiopeiaA10ListedChipRegister : public CasioCassiopeiaA10ChipRegisterBase<kAddress, kInitial> {
public:
    using CasioCassiopeiaA10ChipRegisterBase<kAddress, kInitial>::CasioCassiopeiaA10ChipRegisterBase;

protected:
    bool Accepts(uint16_t value) const override {
        return std::ranges::find(kValues, value) != std::ranges::end(kValues);
    }
};

template <uint32_t kAddress, uint16_t kInitial>
class CasioCassiopeiaA10StoredChipRegister : public CasioCassiopeiaA10ChipRegisterBase<kAddress, kInitial> {
public:
    using CasioCassiopeiaA10ChipRegisterBase<kAddress, kInitial>::CasioCassiopeiaA10ChipRegisterBase;

    uint16_t ReadHalf(uint32_t) override { return this->Value(); }
};

template <uint32_t kAddress, uint16_t kWritableBits>
class CasioCassiopeiaA10MaskedChipRegister : public CasioCassiopeiaA10StoredChipRegister<kAddress, 0u> {
public:
    using CasioCassiopeiaA10StoredChipRegister<kAddress, 0u>::CasioCassiopeiaA10StoredChipRegister;

protected:
    bool Accepts(uint16_t value) const override { return (value & ~kWritableBits) == 0u; }
};

template <uint32_t kAddress, uint16_t kFlagBits>
class CasioCassiopeiaA10FlagsChipRegister : public CasioCassiopeiaA10MaskedChipRegister<kAddress, kFlagBits> {
public:
    using CasioCassiopeiaA10MaskedChipRegister<kAddress, kFlagBits>::CasioCassiopeiaA10MaskedChipRegister;

    void Latch(uint16_t bits) { this->SetBits(static_cast<uint16_t>(bits & kFlagBits)); }

protected:
    void Apply(uint16_t value) override { this->ClearBits(value); }
};

}
