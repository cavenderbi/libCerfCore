#pragma once
#include <cerfcore/core/service.h>
#include <cerfcore/host/widget_control.h>
#include <deque>
#include <mutex>
#include <span>

namespace cerfcore {
class StateWriter;
class StateReader;

class FordSyncGen2Console : public Service {
public:
    using Service::Service;
    bool ShouldRegister() override;
    void OnReady() override;
    std::vector<ActionControl> Actions();
    // Thread-safe frontend entry point. No UART traffic leaves the UI thread.
    bool Tap(uint8_t button);
    void AdjustVolume(int delta);
    void ToggleMute();
    int Volume() const;
    bool Muted() const;
    void ProcessPcm16(std::span<uint8_t> samples) const;
private:
    void Tick();
    void Save(StateWriter& w) const;
    void Restore(StateReader& r);
    void Reset();
    mutable std::mutex mutex_;
    std::deque<uint8_t> pending_;
    bool pressed_ = false;
    int volume_ = 20;
    bool muted_ = false;
};
}
