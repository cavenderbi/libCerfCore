#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <functional>
#include <vector>

namespace cerfcore {

class Vr41xxHsp : public Peripheral {
public:
    using Peripheral::Peripheral;

    void RegisterAferstListener(std::function<void(bool high)> fn);

protected:
    void DriveAferst(bool high);

private:
    std::vector<std::function<void(bool)>> aferst_listeners_;
    bool aferst_high_ = false;
};

}
