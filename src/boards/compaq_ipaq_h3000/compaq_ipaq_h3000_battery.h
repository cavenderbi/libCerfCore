#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/battery.h>

#include <cstdint>
#include <functional>
#include <utility>

namespace cerfcore {

class CompaqIpaqH3000Battery : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    static constexpr uint8_t kReplyLength = 5;

    void Reply(uint8_t* out) const;

    bool OnAc() const;
    bool Charging() const;
    void AddChangeListener(std::function<void()> cb) { battery_.AddChangeListener(std::move(cb)); }

private:
    bool        PcCardPowered() const;
    uint16_t    LoadSag() const;

    Battery       battery_{emu_};
};

}
