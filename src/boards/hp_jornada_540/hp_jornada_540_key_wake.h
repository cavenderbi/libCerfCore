#pragma once

#include <cerfcore/core/service.h>

#include <atomic>

namespace cerfcore {

class Sh77xxIntc;
class StateReader;
class StateWriter;

class HpJornada540KeyWake : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    void Raise();
    void Clear();
    bool Pending() const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    void Publish();

    Sh77xxIntc*       intc_ = nullptr;
    std::atomic<bool> pending_{false};
};

}
