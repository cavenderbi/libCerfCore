#pragma once

#include "../../execution/guest_cycle_clock.h"

#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class ExternalDeviceClock;
class VgaRegisterFile;

class VgaRetrace {
public:
    explicit VgaRetrace(CerfEmulator& emu) : emu_(emu) {}

    void     Attach();
    uint8_t  InputStatus1(const VgaRegisterFile& regs, GuestCycleClock::Rate master_clock) const;
    static uint64_t HostFrameCount(const VgaRegisterFile& regs, GuestCycleClock::Rate master_clock);

private:
    struct Geometry {
        uint64_t char_dots;
        uint64_t line_dots;
        uint64_t frame_lines;
        uint32_t counter_shift;
        uint64_t dot_den;
    };

    static Geometry Measure(const VgaRegisterFile& regs, GuestCycleClock::Rate master_clock);

    CerfEmulator&        emu_;
    ExternalDeviceClock* clock_ = nullptr;
};

}
