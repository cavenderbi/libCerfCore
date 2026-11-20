#pragma once

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

class Vr4101SiuModemLines {
public:
    static constexpr uint16_t kCtsChanged = 1u << 6;
    static constexpr uint16_t kDsrChanged = 1u << 7;
    static constexpr uint16_t kDcdChanged = 1u << 8;

    static constexpr uint16_t kCtsHigh   = 1u << 2;
    static constexpr uint16_t kDsrOn     = 1u << 3;
    static constexpr uint16_t kDcdOn     = 1u << 4;
    static constexpr uint16_t kInputBits = kCtsHigh | kDsrOn | kDcdOn;

    void     SetConnector(bool cts, bool dsr);
    uint16_t Apply(bool powered, bool dcd);
    uint16_t Rs232cInputs() const;

    void Save(StateWriter& w) const;
    void Restore(StateReader& r);

private:
    bool connector_cts_ = false;
    bool connector_dsr_ = false;
    bool cts_on_        = false;
    bool dsr_on_        = false;
    bool dcd_on_        = false;
};

}
