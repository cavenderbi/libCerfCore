#pragma once

#include "../i2c_slave.h"

namespace cerfcore {

class Pxa27xI2cDevice : public I2cSlave {
public:
    using I2cSlave::I2cSlave;
};

}
