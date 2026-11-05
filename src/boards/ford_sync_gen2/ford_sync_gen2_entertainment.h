#pragma once

#include <cerfcore/core/service.h>
#include "ford_sync_gen2_entertainment_state.h"
#include "ford_sync_gen2_ilp_channel.h"
#include <mutex>

namespace cerfcore {

class FordSyncGen2Entertainment : public Service {
public:
    using Service::Service;
    bool ShouldRegister() override;
    void OnReady() override;
    FordSyncGen2EntertainmentState Snapshot() const;
private:
    using State = FordSyncGen2EntertainmentState;
    using Channel = FordSyncGen2IlpChannel;
    mutable std::mutex mutex_;
    State state_;
    Channel::Result Apply(const std::vector<Channel::Write>& writes);
    void Refresh(bool force);
    void Save(StateWriter& w) const;
    void Restore(StateReader& r);
};

}
