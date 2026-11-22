#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kSh77xxIntcArea7Base = 0x1FFFFEE0u;
inline constexpr uint32_t kSh77xxIntcArea7Size = 6u;

enum class Sh77xxIntcSource : uint8_t {
    kDei0,
    kDei1,
    kDei2,
    kDei3,
    kEri1,
    kRxi1,
    kBri1,
    kTxi1,
    kEri2,
    kRxi2,
    kBri2,
    kTxi2,
    kAdi,
    kLcdi,
    kPc0Bdi,
    kPc0Bwi,
    kPc0Rci,
    kPc0Cdi,
    kPc0Sci,
    kPc0Iri,
    kPc0Swi,
    kPc1Bdi,
    kPc1Bwi,
    kPc1Rci,
    kPc1Cdi,
    kPc1Swi,
    kTuni0,
    kTuni1,
    kTuni2,
    kTicpi2,
    kAti,
    kPri,
    kCui,
    kEri,
    kRxi,
    kTxi,
    kTei,
    kIti,
    kRcmi,
    kRovi,
    kIntevt6C0,
    kIntevt6E0,
    kIntevt700,
    kIntevt720,
    kIntevt740,
    kIntevt760,
    kCount,
};

enum class Sh77xxIntcPin : uint8_t {
    kIrq0,
    kIrq1,
    kIrq2,
    kIrq3,
    kIrq4,
    kIrq5,
    kPint0,
    kPint8  = kPint0 + 8,
    kPint15 = kPint0 + 15,
    kIrls0,
    kIrls3 = kIrls0 + 3,
    kCount,
};

constexpr Sh77xxIntcPin Sh77xxPintPin(uint32_t pint) {
    return static_cast<Sh77xxIntcPin>(static_cast<uint32_t>(Sh77xxIntcPin::kPint0) + pint);
}

class Sh77xxIntc : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual void SetSourceRequest(Sh77xxIntcSource source, bool asserted)                             = 0;
    virtual void SetPinLevel(Sh77xxIntcPin pin, bool high)                                            = 0;
    virtual void SetPortDrive(uint32_t driven_pins, uint32_t driven_high, uint32_t absent_input_pins,
                              uint32_t absent_irl_pins)                                               = 0;
    virtual bool PinLevel(Sh77xxIntcPin pin)                                                          = 0;
    virtual void SetNmiLevel(bool high)                                                               = 0;
    virtual bool WakesStandby(Sh77xxIntcSource source)                                                = 0;
    virtual bool PintWakesStandby(uint32_t pint, bool high)                                           = 0;
    virtual void SetStandbyWake(Sh77xxIntcSource source, bool wakes)                                  = 0;

    virtual uint16_t ReadArea7Half(uint32_t addr)                  = 0;
    virtual void     WriteArea7Half(uint32_t addr, uint16_t value) = 0;
};

}
