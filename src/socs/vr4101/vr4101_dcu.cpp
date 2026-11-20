#include "../vr41xx/vr41xx_aiu_dma_window.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>

#include <atomic>
#include <cstdint>
#include "vr4101_dma_control.h"
#include "vr4101_id.h"

namespace cerfcore {

namespace {

using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

constexpr uint32_t kRstIndex  = 0u;
constexpr uint32_t kIdleIndex = 1u;
constexpr uint32_t kSenIndex  = 2u;
constexpr uint32_t kMskIndex  = 3u;
constexpr uint32_t kReqIndex  = 4u;

constexpr uint16_t kChannelBits = 0x001Fu;

constexpr RegWindowModel kModel = {
    0x0B000040u,
    0x20u,
    5u,
    true,
    {
        { ReadKind::kStored, WriteKind::kStored, 0x0001u, 0x0000u },
        { ReadKind::kSampled, WriteKind::kFatal },
        { ReadKind::kStored, WriteKind::kStored, 0x0001u, 0x0000u },
        { ReadKind::kStored, WriteKind::kStored, kChannelBits, 0x0000u },
        { ReadKind::kSampled, WriteKind::kFatal },
    },
};

class Vr4101Dcu : public Vr41xxAiuDmaWindow<SocId::Vr4101, kModel, Vr4101DmaControl> {
public:
    using Vr41xxAiuDmaWindow::Vr41xxAiuDmaWindow;

    bool TransferEnabled(Vr4101DmaChannel channel) const override {
        return Idle() && (StoredReg(kMskIndex) & ChannelBit(channel)) != 0u;
    }

    void SetRequest(Vr4101DmaChannel channel, bool requested) override {
        if (requested) requests_.fetch_or(ChannelBit(channel), std::memory_order_acq_rel);
        else           requests_.fetch_and(static_cast<uint16_t>(~ChannelBit(channel)), std::memory_order_acq_rel);
    }

    void SaveState(StateWriter& w) override {
        Window::SaveState(w);
        w.Write<uint16_t>("dma_requests", requests_.load(std::memory_order_acquire));
    }

    void RestoreState(StateReader& r) override {
        Window::RestoreState(r);
        uint16_t requests = 0u;
        r.Read("dma_requests", requests);
        requests_.store(requests, std::memory_order_release);
    }

protected:
    void AfterReset() override { requests_.store(0u, std::memory_order_release); }

    uint16_t SampleRegister(uint32_t i, uint32_t addr) override {
        if (i == kIdleIndex) return Idle() ? 1u : 0u;
        if (i == kReqIndex) {
            return static_cast<uint16_t>(requests_.load(std::memory_order_acquire) |
                                         emu_.Get<Vr41xxAiuDma>().DmaRequests());
        }
        HaltUnsupportedAccess("Vr4101Dcu sampled read", addr, 0);
    }

    void BeforeStore(uint32_t i, uint16_t value) override {
        enabled_before_ = EnabledBits();
        if (i != kMskIndex) return;
        const uint16_t rising = static_cast<uint16_t>(value & ~StoredReg(kMskIndex));
        if (rising != 0u && !Idle()) {
            emu_.Get<Fatal>().Die("Vr4101Dcu: DMAMSKREG write 0x%04X enables a transfer outside D_IDLE "
                                  "(DMASEN 0x%04X, DMARST 0x%04X)", value, StoredReg(kSenIndex), StoredReg(kRstIndex));
        }
    }

    void AfterStore(uint32_t) override {
        const uint16_t rising = static_cast<uint16_t>(EnabledBits() & ~enabled_before_);
        if (rising != 0u) TransfersEnabled(rising);
    }

private:
    uint16_t EnabledBits() const { return Idle() ? static_cast<uint16_t>(StoredReg(kMskIndex) & kChannelBits) : 0u; }

    bool Idle() const {
        return (StoredReg(kSenIndex) & 1u) != 0u && (StoredReg(kRstIndex) & 1u) == 0u;
    }

    std::atomic<uint16_t> requests_{0u};
    uint16_t              enabled_before_ = 0u;
};

}

REGISTER_SERVICE_AS(Vr4101Dcu, Vr4101DmaControl);

}
