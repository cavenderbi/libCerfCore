#pragma once

#include "../../core/service.h"
#include "../../state/state_stream.h"

#include <atomic>

namespace cerfcore {

class CerfVirtCustomizationsReset : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    void OnCustomizationsApplied();

    void Invalidate();

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    std::atomic<bool> applied_{false};
};

}
