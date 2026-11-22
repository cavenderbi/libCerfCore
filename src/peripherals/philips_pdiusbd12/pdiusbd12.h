#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../external_device_clock.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cerfcore {

class Pdiusbd12Config;

class Pdiusbd12 : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override { return base_; }
    uint32_t MmioSize() const override { return size_; }

    uint8_t ReadByte(uint32_t addr) override;
    void    WriteByte(uint32_t addr, uint8_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

    void SetSuspendHeldLow(bool held);
    bool SuspendPinHigh() const { return suspended_ != 0u && held_low_ == 0u; }

    static constexpr size_t kBufferBytes = 130u;

    struct Buffer {
        std::array<uint8_t, kBufferBytes> bytes{};
        uint8_t                           full = 0u;
    };

    struct Endpoint {
        std::array<Buffer, 2> buffers{};
        uint8_t               cpu_buffer = 0u;
        uint8_t               stalled    = 0u;
    };

private:
    template <class F>
    static constexpr void VisitEndpoint(Endpoint& e, F& field) {
        field("buffer0_bytes", e.buffers[0].bytes);
        field("buffer0_full", e.buffers[0].full);
        field("buffer1_bytes", e.buffers[1].bytes);
        field("buffer1_full", e.buffers[1].full);
        field("cpu_buffer", e.cpu_buffer);
        field("stalled", e.stalled);
    }

    void     PowerOnReset();
    uint8_t  Mode() const;
    bool     Exists(size_t index) const;
    size_t   PacketBytes(size_t index) const;
    void     RequireEndpoint(uint32_t addr, size_t index);
    void     Command(uint32_t addr, uint8_t value);
    void     ValidateBuffer(uint32_t addr);
    void     ClearBuffer(uint32_t addr);
    uint8_t  ReadData(uint32_t addr);
    uint8_t  ReadInterrupt(uint8_t phase);
    uint8_t  ReadTransactionStatus(size_t index);
    void     WriteData(uint32_t addr, uint8_t value);
    void     SetEndpointStatus(uint32_t addr, size_t index, uint8_t value);
    size_t   BufferIndex(uint32_t addr);
    uint8_t  ReadBuffer(uint32_t addr);
    void     WriteBuffer(uint32_t addr, uint8_t value);
    void     SetInterrupt(uint16_t bits);
    void     ClearInterrupt(uint16_t bits);
    void     PublishInterrupt();
    void     EnterSuspend();
    void     Wake();
    void     ArmSuspend();
    int64_t  ParkWakeDueNs();

    Pdiusbd12Config*            config_        = nullptr;
    ExternalDeviceClock*        clock_         = nullptr;
    ExternalDeviceClock::Timer* suspend_timer_ = nullptr;
    uint32_t                base_            = 0u;
    uint32_t                size_            = 0u;
    uint8_t                 command_         = 0u;
    uint8_t                 phase_           = 0u;
    uint8_t                 configuration_   = 0u;
    uint8_t                 clock_division_  = 0u;
    uint8_t                 dma_             = 0u;
    uint8_t                 address_enable_  = 0u;
    uint8_t                 endpoint_enable_ = 0u;
    uint8_t                 selected_        = 0u;
    uint8_t                 pointer_         = 0u;
    uint16_t                interrupt_       = 0u;
    uint8_t                 suspended_       = 0u;
    uint8_t                 held_low_        = 0u;
    std::array<Endpoint, 6> endpoints_{};
};

}
