#pragma once

#include <cerfcore/core/service.h>

#include <optional>
#include <string>

namespace cerfcore {

struct Vr41xxSerialModem {
    Vr41xxSerialModem(std::string label_in, int carrier_gpio_in, int supply_gpio_in);

    std::string label;
    int         carrier_gpio;
    int         supply_gpio;
};

class Vr41xxSerialWiring : public Service {
public:
    using Service::Service;

    virtual std::optional<Vr41xxSerialModem> ForSiu() const = 0;
};

}
