#include "sh77xx_serial_format.h"

namespace cerfcore {

namespace {

constexpr uint8_t  kModeChr          = 0x40u;
constexpr uint8_t  kModeParity       = 0x20u;
constexpr uint8_t  kModeOdd          = 0x10u;
constexpr uint8_t  kModeTwoStop      = 0x08u;
constexpr uint8_t  kModeClock        = 0x03u;
constexpr uint64_t kClocksPerBitBase = 32u;

}

uint64_t Sh77xxSerialBitPclkTicks(uint8_t smr, uint8_t brr) {
    return (kClocksPerBitBase << (2u * (smr & kModeClock))) * (static_cast<uint64_t>(brr) + 1u);
}

GuestCycleClock::Rate Sh77xxSerialBitRate(uint8_t smr, uint8_t brr, GuestCycleClock::Rate pclk) {
    return pclk.Scaled(1u, Sh77xxSerialBitPclkTicks(smr, brr));
}

uint32_t Sh77xxSerialBaud(uint8_t smr, uint8_t brr, GuestCycleClock::Rate pclk) {
    const GuestCycleClock::Rate bit = Sh77xxSerialBitRate(smr, brr, pclk);
    return static_cast<uint32_t>(bit.num / bit.den);
}

SerialLine::LineConfig Sh77xxSerialFrameFormat(uint8_t smr) {
    SerialLine::LineConfig c;
    c.data_bits = (smr & kModeChr) != 0u ? 7u : 8u;
    if ((smr & kModeParity) == 0u) {
        c.parity = SerialLine::LineConfig::Parity::None;
    } else {
        c.parity = (smr & kModeOdd) != 0u ? SerialLine::LineConfig::Parity::Odd : SerialLine::LineConfig::Parity::Even;
    }
    c.stop = (smr & kModeTwoStop) != 0u ? SerialLine::LineConfig::Stop::Two : SerialLine::LineConfig::Stop::One;
    return c;
}

}
