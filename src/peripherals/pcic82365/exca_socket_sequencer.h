#pragma once

#include "pcic82365.h"

#include <array>
#include <mutex>

namespace cerfcore {

class PcmciaSlot;

struct ExcaSocketUpdate {
    PcmciaSlot*     slot = nullptr;
    ExcaWriteEffect effect;
};

using ExcaSocketUpdates = std::array<ExcaSocketUpdate, 2>;

class ExcaSocketSequencer {
public:
    template <class Compute>
    void Run(Compute&& compute) {
        std::lock_guard<std::mutex> lk(mutex_);
        const ExcaSocketUpdates updates = compute();
        for (const ExcaSocketUpdate& u : updates) {
            if (u.slot) Apply(*u.slot, u.effect);
        }
    }

private:
    static void Apply(PcmciaSlot& slot, const ExcaWriteEffect& effect);

    std::mutex mutex_;
};

}
