#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

#include <cstdint>

namespace cerfcore {

enum class CasioCassiopeiaA10Vdet : uint8_t {
    kVdet1,
    kVdet2,
    kVdet3,
};

class CasioCassiopeiaA10Battery : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    bool OnExternalPower() const;
    bool Recharging() const;
    bool Tripped(CasioCassiopeiaA10Vdet detector) const;
    void AddChangeListener(std::function<void()> listener);

private:
    Battery battery_{emu_};
};

}
