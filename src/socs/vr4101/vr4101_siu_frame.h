#pragma once

#include "../../peripherals/serial/serial_line.h"

#include <cstdint>

namespace cerfcore::vr4101_siu {

using Parity = SerialLine::LineConfig::Parity;

inline constexpr uint16_t kDLenWritable = 0x01FFu;
inline constexpr uint16_t kBaudSelField = 0x0007u;

struct TxChar {
    uint8_t data  = 0u;
    bool    ninth = false;
};

uint32_t RxBits(uint16_t dlength);
uint32_t TxBits(uint16_t dlength);
uint32_t Baud(uint16_t baudsel);
bool     FrameValid(uint16_t dlength);
bool     NineBitPayload(uint16_t dlength);

SerialLine::LineConfig LineConfigOf(uint16_t dlength, uint16_t baudsel, Parity nine_bit_parity);
uint16_t               ReceivedWord(uint16_t dlength, uint16_t baudsel, Parity nine_bit_parity, uint8_t byte);
bool                   TransmittedChar(uint16_t dlength, uint16_t word, TxChar& out);
Parity                 ParityCarrying(const TxChar& c, Parity current);

}
