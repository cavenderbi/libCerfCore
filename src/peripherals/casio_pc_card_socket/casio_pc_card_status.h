#pragma once

#include <atomic>
#include <cstdint>

namespace cerfcore {

class PcmciaSlot;
class StateReader;
class StateWriter;

class CasioPcCardStatus {
public:
    static constexpr uint16_t kReady      = 0x0001u;
    static constexpr uint16_t kCardAbsent = 0x0006u;
    static constexpr uint16_t kBvd1       = 0x0008u;

    void     SetReadyIreqLow(bool low);
    bool     ReadyIreqLow() const;
    uint16_t Word(const PcmciaSlot& slot) const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    std::atomic<bool> ready_ireq_low_{false};
};

}
