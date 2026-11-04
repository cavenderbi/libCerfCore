#pragma once

#include "serial_line.h"

#include <cstdint>

namespace cerfcore {

struct UartDecodedFrame {
    uint8_t data          = 0;
    bool    parity_error  = false;
    bool    framing_error = false;
};

bool     UartParityBit(SerialLine::LineConfig::Parity parity, uint8_t data, uint8_t data_bits);
uint32_t UartFrameBits(const SerialLine::LineConfig& format);
uint32_t UartFrameLevels(const SerialLine::LineConfig& format, uint8_t data);

UartDecodedFrame DecodeUartFrame(const SerialLine::LineConfig& sent, uint8_t data,
                                 const SerialLine::LineConfig& received);

}
