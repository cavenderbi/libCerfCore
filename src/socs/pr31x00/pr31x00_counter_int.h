#pragma once

#include <cerfcore/core/service.h>
#include "../../execution/guest_cycle_clock.h"
#include "pr31x00_intc.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace cerfcore {

class StateReader;
class StateWriter;

class Pr31x00CounterTimeline {
public:
    virtual uint64_t CountAt(uint64_t cycle)                     = 0;
    virtual bool     NextCountCycle(uint64_t cycle, uint64_t& at) = 0;
    virtual void     Settle()                                    = 0;

protected:
    ~Pr31x00CounterTimeline() = default;
};

class Pr31x00CounterInt : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t Attach(uint32_t set, uint32_t bit, Pr31x00CounterTimeline& timeline);
    void     Rearm(uint32_t port);

    void SaveState(uint32_t port, StateWriter& w) const;
    void RestoreState(uint32_t port, StateReader& r);

private:
    class Channel final : public Pr31x00IntcTimedSource {
    public:
        Channel(Pr31x00CounterInt& owner, uint32_t set, uint32_t bit, Pr31x00CounterTimeline& timeline);

        void Settle() override;
        bool EdgeSinceClear() const override;
        void OnLatched() override;
        void OnStatusCleared() override;
        void OnPathChanged(bool open) override;

        void Rearm();
        void Latch();
        void Save(StateWriter& w) const;
        void Restore(StateReader& r);

    private:
        Pr31x00CounterInt&      owner_;
        const uint32_t          set_;
        const uint32_t          bit_;
        Pr31x00CounterTimeline& timeline_;
        GuestCycleClock::Event* event_   = nullptr;
        uint64_t                cleared_ = 0;
        bool                    latched_ = false;
        bool                    open_    = false;
    };

    std::vector<std::unique_ptr<Channel>> ch_;

    GuestCycleClock* clock_ = nullptr;
    Pr31x00Intc*     intc_  = nullptr;
};

}
