#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_power_input.h>

#include <atomic>
#include <string>

namespace cerfcore {

class Sh77xxIntc;

class Sh77xxNmiPowerFailInput : public Service, public MachinePowerInput {
public:
    using Service::Service;

    static constexpr char kObjectName[] = "Main power";

    void OnReady() override;

    std::string ObjectId() const override { return "main_power"; }
    std::string ObjectName() const override { return kObjectName; }
    bool        PowerFailed() const override { return failed_.load(std::memory_order_acquire); }
    void        SetPowerFailed(bool failed) override;

    void SaveObjectState(StateWriter& w) const override;
    void RestoreObjectState(StateReader& r) override;

private:
    Sh77xxIntc*       intc_ = nullptr;
    std::atomic<bool> failed_{false};
};

}
