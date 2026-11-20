#pragma once

#include <cerfcore/core/callback_list.h>
#include "serial_device_clock_timer.h"
#include "serial_endpoint.h"
#include "serial_endpoint_port.h"
#include "serial_line.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace cerfcore {

class CerfEmulator;
class StateWriter;
class StateReader;

class SerialLineSwitch : public SerialLine {
public:
    SerialLineSwitch(CerfEmulator& emu, uint32_t lines);

    void           Attach(ObjectIdentity identity);
    MachineObject* Activity() const;
    void           AttachLine(uint32_t index, SerialLine& line);
    void Select(std::optional<uint32_t> index, std::optional<uint32_t> joined);
    void AddInputsListener(CallbackList<bool, bool, bool, bool>::Callback listener);
    void OnShutdown();

    void SaveSwitchState(StateWriter& w);
    void RestoreSwitchState(StateReader& r);
    void PostRestore();

    void       PushRx(const uint8_t* data, size_t n) override;
    bool       RxEmpty() const override;
    void       SetRxDrainCallback(RxDrainFn cb) override;
    void       SetModemInputs(bool cts, bool dsr, bool ri, bool dcd) override;
    LineConfig GetLineConfig() const override;
    void       SetLineConfigCallback(LineConfigFn cb) override;
    void       SetEndpoint(SerialEndpoint* ep) override;
    void       ArmEndpointTimer(int64_t delay_ns) override;
    void       DisarmEndpointTimer() override;

private:
    class Tap : public SerialEndpoint {
    public:
        Tap(SerialLineSwitch& owner, uint32_t index) : owner_(owner), index_(index) {}

        void OnGuestTx(const uint8_t* data, size_t n) override;
        void OnControlLines(bool dtr, bool rts) override;
        void OnBreak(bool on) override;
        void ResendModemInputs() override;

        bool InBreak() const { return in_break_.load(std::memory_order_acquire); }
        void ClearBreak() { in_break_.store(false, std::memory_order_release); }

    private:
        bool Selected() const;

        SerialLineSwitch& owner_;
        uint32_t          index_;
        std::atomic<bool> in_break_{false};
    };

    static constexpr uint8_t  kCts      = 0x01u;
    static constexpr uint8_t  kDsr      = 0x02u;
    static constexpr uint8_t  kRi       = 0x04u;
    static constexpr uint8_t  kDcd      = 0x08u;
    static constexpr uint32_t kNoneLine = UINT32_MAX;

    SerialControlLines ControlLines() const;
    SerialLine*        LineAt(std::optional<uint32_t> index) const;
    void               ConnectLocked(SerialLine& line, uint32_t index);
    void               DisconnectLocked(SerialLine& line, uint32_t index);

    CerfEmulator&                     emu_;
    std::vector<SerialLine*>          lines_;
    std::vector<std::unique_ptr<Tap>> taps_;
    std::recursive_mutex              route_mu_;
    std::optional<uint32_t>           selected_;
    std::optional<uint32_t>           joined_;
    bool                              plugged_ = false;
    std::atomic<SerialLine*>          selected_line_{nullptr};
    std::atomic<uint32_t>             selected_index_{kNoneLine};
    std::atomic<uint8_t>              inputs_{0u};
    CallbackList<bool, bool, bool, bool> inputs_listeners_;
    std::atomic<bool>                 dtr_{false};
    std::atomic<bool>                 rts_{false};
    RxDrainFn                         drain_;
    LineConfigFn                      line_config_;
    SerialDeviceClockTimer            timer_;
    SerialEndpointPort                port_;
};

}
