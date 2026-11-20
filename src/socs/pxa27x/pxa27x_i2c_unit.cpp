#include "pxa27x_i2c_unit.h"

namespace cerfcore {

bool Pxa27xI2cUnit::Read(uint32_t off, uint32_t& value) const {
    switch (off) {
        case kIBMR: value = IntelI2cUnit::kIbmrIdle; return true;
        case kIDBR: value = unit_.Idbr(); return true;
        case kICR:  value = unit_.Icr(); return true;
        case kISR:  value = unit_.Isr(); return true;
        case kISAR: value = unit_.Isar(); return true;
    }
    return false;
}

bool Pxa27xI2cUnit::Write(uint32_t off, uint32_t value, I2cSlave* slave) {
    switch (off) {
        case kIDBR: unit_.WriteIdbr(value); return true;
        case kISR:  unit_.WriteIsr(value); return true;
        case kISAR: unit_.WriteIsar(value); return true;
        case kICR:  unit_.WriteIcr(value, slave); return true;
    }
    return false;
}

}
