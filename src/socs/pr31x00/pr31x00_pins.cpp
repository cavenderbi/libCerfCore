#include "pr31x00_pins.h"

namespace cerfcore {

uint32_t Pr31x00Pins::Low() const { return (driven & ~level) | pulled_down; }

}
