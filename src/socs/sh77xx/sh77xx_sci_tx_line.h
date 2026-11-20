#pragma once

#include "../../execution/guest_cycle_clock.h"
#include "../rated_tick_count.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace cerfcore {

class CerfEmulator;
class Sh77xxCpg;
class Sh77xxFrqcr;
class StateReader;
class StateWriter;

class Sh77xxSciTxLine {
public:
    explicit Sh77xxSciTxLine(CerfEmulator& emu) : emu_(emu), bits_(emu) {}

    void Attach(std::function<void()> on_due);

    void    Initialize();
    uint8_t Data() const;
    uint8_t Flags() const;
    void    Write(uint8_t value);
    bool    ClearEmpty(uint8_t smr, uint8_t brr, bool enabled);
    uint8_t Disable();
    uint8_t Settle();
    void    TakeSent(std::vector<uint8_t>& out);
    void    SetInterrupts(bool empty_irq, bool end_irq);
    bool    Reformat(uint8_t smr, uint8_t brr);
    bool    Rescale();

    void Save(StateWriter& w) const;
    bool Restore(StateReader& r);

private:
    void Shape(uint8_t smr, uint8_t brr);
    void Load(uint64_t frame_start);
    void Stop();
    void Arm();

    CerfEmulator&           emu_;
    GuestCycleClock*        clock_ = nullptr;
    Sh77xxCpg*              cpg_   = nullptr;
    Sh77xxFrqcr*            frqcr_ = nullptr;
    GuestCycleClock::Event* event_ = nullptr;

    uint8_t              tdr_   = 0u;
    uint8_t              flags_ = 0u;
    std::vector<uint8_t> sent_;

    RatedTickCount        bits_;
    GuestCycleClock::Rate bit_rate_{};
    bool                  seven_bit_ = false;
    bool                  timed_     = false;
    bool                  pending_   = false;
    bool                  shifting_  = false;
    bool                  empty_irq_ = false;
    bool                  end_irq_   = false;
    uint64_t              load_tick_ = 0u;
    uint64_t              stop_tick_ = 0u;
    uint64_t              free_tick_ = 0u;
    uint32_t              to_stop_   = 0u;
    uint32_t              stop_bits_ = 0u;
};

}
