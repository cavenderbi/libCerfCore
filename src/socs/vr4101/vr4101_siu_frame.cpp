#include "vr4101_siu_frame.h"

#include "../../peripherals/serial/uart_frame.h"

#include <algorithm>
#include <array>

namespace cerfcore::vr4101_siu {

namespace {

constexpr uint16_t kDLenTxBits  = 0x000Fu;
constexpr uint16_t kDLenTwoStop = 1u << 8;
constexpr uint32_t kDLenRxShift = 4u;
constexpr uint16_t kDLenRxField = 0x000Fu;
constexpr uint32_t kRxBitsMin   = 8u;
constexpr uint32_t kRxBitsMax   = 10u;
constexpr uint32_t kWordBits    = 16u;
constexpr uint32_t kByteBits    = 8u;

constexpr std::array<uint32_t, 8> kBauds = {1200u, 2400u, 4800u, 9600u, 19200u, 38400u, 57600u, 115200u};

uint32_t StopBits(uint16_t dlength) { return (dlength & kDLenTwoStop) != 0u ? 2u : 1u; }
uint32_t Payload(uint16_t dlength) { return RxBits(dlength) - 1u; }
uint32_t DataBits(uint16_t dlength) { return std::min(Payload(dlength), kByteBits); }

}

uint32_t RxBits(uint16_t dlength) { return (dlength >> kDLenRxShift) & kDLenRxField; }
uint32_t TxBits(uint16_t dlength) { return dlength & kDLenTxBits; }
uint32_t Baud(uint16_t baudsel) { return kBauds[baudsel & kBaudSelField]; }

bool FrameValid(uint16_t dlength) {
    const uint32_t rx = RxBits(dlength);
    return rx >= kRxBitsMin && rx <= kRxBitsMax && TxBits(dlength) == rx + StopBits(dlength);
}

bool NineBitPayload(uint16_t dlength) { return FrameValid(dlength) && Payload(dlength) > kByteBits; }

SerialLine::LineConfig LineConfigOf(uint16_t dlength, uint16_t baudsel, Parity nine_bit_parity) {
    SerialLine::LineConfig cfg;
    cfg.baud = Baud(baudsel);
    if (!FrameValid(dlength)) return cfg;
    cfg.data_bits = static_cast<uint8_t>(DataBits(dlength));
    cfg.parity    = NineBitPayload(dlength) ? nine_bit_parity : Parity::None;
    cfg.stop      = StopBits(dlength) == 2u ? SerialLine::LineConfig::Stop::Two : SerialLine::LineConfig::Stop::One;
    return cfg;
}

uint16_t ReceivedWord(uint16_t dlength, uint16_t baudsel, Parity nine_bit_parity, uint8_t byte) {
    const uint32_t rx_bits = RxBits(dlength);
    const uint32_t levels  = UartFrameLevels(LineConfigOf(dlength, baudsel, nine_bit_parity), byte) &
                            ((1u << rx_bits) - 1u);
    const uint32_t below = kWordBits - rx_bits;
    return static_cast<uint16_t>((levels << below) | ((1u << below) - 1u));
}

bool TransmittedChar(uint16_t dlength, uint16_t word, TxChar& out) {
    if ((word & 1u) != 0u) return false;
    const uint32_t data_bits = DataBits(dlength);
    out.data  = static_cast<uint8_t>((word >> 1) & ((1u << data_bits) - 1u));
    out.ninth = NineBitPayload(dlength) && ((word >> (1u + kByteBits)) & 1u) != 0u;
    return true;
}

Parity ParityCarrying(const TxChar& c, Parity current) {
    if (UartParityBit(current, c.data, kByteBits) == c.ninth) return current;
    return current == Parity::Even ? Parity::Odd : Parity::Even;
}

}
