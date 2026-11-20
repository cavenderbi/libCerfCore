#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

#include "../../socs/reg_window_impl.h"

namespace cerfcore {

class Hd64461SocketWiring;

inline constexpr cerf_reg_window_detail::RegWindowModel kHd64461SystemControlModel = {
    0x10000000u,
    6u,
    3u,
    false,
    {
        {cerf_reg_window_detail::ReadKind::kStored, cerf_reg_window_detail::WriteKind::kStored, 0x3FFFu, 0x0000u,
         0x0000u, cerf_reg_window_detail::OtherReset::kReset, 0x0000u, 0x0000u},
        {cerf_reg_window_detail::ReadKind::kSampled, cerf_reg_window_detail::WriteKind::kStored, 0x20F0u, 0x0000u,
         0xDF0Cu, cerf_reg_window_detail::OtherReset::kReset, 0x0000u, 0x0000u},
        {cerf_reg_window_detail::ReadKind::kStored, cerf_reg_window_detail::WriteKind::kStored, 0xFFFFu, 0x0000u,
         0xAA00u, cerf_reg_window_detail::OtherReset::kReset, 0x0000u, 0x0000u},
    },
    2u,
    false,
    true,
};

class Hd64461SystemControl
    : public cerf_reg_window_detail::RegWindow<kHd64461SystemControlModel, Peripheral, std::mutex> {
public:
    using RegWindow::RegWindow;

    bool ShouldRegister() override;
    void OnReady() override;

    void RestoreState(StateReader& r) override;

    bool LcdcClockRunning() const;
    bool PccChannelClockRunning(uint32_t channel) const;
    bool TimerClockRunning(uint32_t channel) const;
    bool TimerOutputsSelected() const;
    bool CpuBusInputsGated() const;

    uint8_t  ReadByte(uint32_t addr) override;
    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t value) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

protected:
    uint16_t SampleRegister(uint32_t i, uint32_t addr) override;
    void     BeforeStore(uint32_t i, uint16_t value) override;
    void     AfterStore(uint32_t i) override;
    void     AfterReset() override;

private:
    void PublishLocked();
    void ApplyCpuBusGateLocked();
    void PassCpuBus(const char* access, uint32_t addr, uint32_t value, bool gated_store) const;

    const Hd64461SocketWiring* wiring_        = nullptr;
    std::atomic<uint16_t>      stbcr_{0u};
    std::atomic<uint16_t>      syscr_{0u};
    uint32_t                   register_gate_ = 0u;
    uint32_t                   card_gate_     = 0u;
};

}
