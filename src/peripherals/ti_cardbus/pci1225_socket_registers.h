#pragma once

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

struct Pci1225CardPins {
    bool    present;
    uint8_t interface_status;
    bool    vs1_grounded;
    bool    vs2_grounded;
    bool    low_voltage_key;
    bool    powered;
};

struct Pci1225PowerRequest {
    bool vcc_on;
    bool request_changed;
    bool vpp12_crossed;
};

class Pci1225SocketRegisters {
public:
    static constexpr uint32_t kEventCstsChange = 0x01u;
    static constexpr uint32_t kEventCardDetect = 0x06u;
    static constexpr uint32_t kEventPowerCycle = 0x08u;

    void Reset();

    bool EventPending() const { return (event_ & mask_ & kEventsAll) != 0u; }
    void Latch(uint32_t events) { event_ |= events & kEventsAll; }
    void NoteSocketAccess() { socket_access_ = true; }
    void Interrogate() { forced_ = 0u; }

    static bool IsControl(uint32_t off);

    bool        ReadWindow(uint32_t off, unsigned size, const Pci1225CardPins& pins, uint32_t* value);
    uint32_t    MergedControl(uint32_t off, uint32_t value, unsigned size) const;
    const char* WriteWindow(uint32_t off, uint32_t value, unsigned size);
    bool     WriteControl(uint32_t value, Pci1225PowerRequest* request);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    static constexpr uint32_t kEventsAll = 0x0Fu;

    uint32_t PresentState(const Pci1225CardPins& pins) const;
    static uint32_t CardVoltageBits(const Pci1225CardPins& pins);
    bool     Read(uint32_t reg, const Pci1225CardPins& pins, uint32_t* value);
    bool     WriteMask(uint32_t value);
    void     WriteForceEvent(uint32_t value);

    uint32_t event_         = 0u;
    uint32_t mask_          = 0u;
    uint32_t control_       = 0u;
    uint32_t power_mgmt_    = 0u;
    uint32_t forced_        = 0u;
    bool     bad_vcc_       = false;
    bool     socket_access_ = false;
};

}
