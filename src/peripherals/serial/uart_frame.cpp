#include "uart_frame.h"

#include <bit>

namespace cerfcore {

using Parity = SerialLine::LineConfig::Parity;

bool UartParityBit(Parity parity, uint8_t data, uint8_t data_bits) {
    const unsigned mask     = (1u << data_bits) - 1u;
    const bool     odd_ones = (std::popcount(static_cast<unsigned>(data) & mask) & 1) != 0;
    switch (parity) {
        case Parity::Odd:   return !odd_ones;
        case Parity::Even:  return odd_ones;
        case Parity::Mark:  return true;
        case Parity::Space: return false;
        case Parity::None:  break;
    }
    return true;
}

uint32_t UartFrameBits(const SerialLine::LineConfig& format) {
    const uint32_t parity = format.parity != Parity::None ? 1u : 0u;
    const uint32_t stop   = format.stop == SerialLine::LineConfig::Stop::One ? 1u : 2u;
    return 1u + format.data_bits + parity + stop;
}

uint32_t UartFrameLevels(const SerialLine::LineConfig& format, uint8_t data) {
    const uint32_t value = data & ((1u << format.data_bits) - 1u);
    uint32_t       bits  = value << 1;
    uint32_t       pos   = 1u + format.data_bits;
    if (format.parity != Parity::None) {
        if (UartParityBit(format.parity, data, format.data_bits)) bits |= 1u << pos;
        ++pos;
    }
    for (const uint32_t end = UartFrameBits(format); pos < end; ++pos) bits |= 1u << pos;
    return bits;
}

UartDecodedFrame DecodeUartFrame(const SerialLine::LineConfig& sent, uint8_t data,
                                 const SerialLine::LineConfig& received) {
    const uint32_t idle  = ~((1u << UartFrameBits(sent)) - 1u);
    const uint32_t line  = UartFrameLevels(sent, data) | idle;
    const auto     level = [line](uint32_t i) { return ((line >> i) & 1u) != 0u; };

    UartDecodedFrame frame;
    uint32_t         at = 1u;
    for (uint8_t i = 0; i < received.data_bits; ++i, ++at) {
        if (level(at)) frame.data = static_cast<uint8_t>(frame.data | (1u << i));
    }
    if (received.parity != Parity::None) {
        const bool sampled = level(at++);
        frame.parity_error = sampled != UartParityBit(received.parity, frame.data, received.data_bits);
    }
    frame.framing_error = !level(at);
    return frame;
}

}
