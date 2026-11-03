#include "vr41xx_serial_wiring.h"

#include <utility>

namespace cerfcore {

Vr41xxSerialModem::Vr41xxSerialModem(std::string label_in, int carrier_gpio_in,
                                     int supply_gpio_in)
    : label(std::move(label_in)), carrier_gpio(carrier_gpio_in), supply_gpio(supply_gpio_in) {}

}
