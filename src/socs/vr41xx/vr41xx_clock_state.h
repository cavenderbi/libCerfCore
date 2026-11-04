#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

class Vr41xxClockState : public Service {
public:
    using Service::Service;

    void OnReady() override;

    const char* WritePending(uint16_t value);
    uint16_t    Pending() const { return pending_; }

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

protected:
    virtual const char* RejectedPending(uint16_t value) const = 0;
    virtual void        ActiveRestored() {}

    uint16_t Active() const { return active_; }

private:
    uint16_t pending_ = 0;
    uint16_t active_  = 0;
};

}
