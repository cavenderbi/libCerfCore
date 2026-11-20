#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cerfcore {

class StateReader;
class StateWriter;

class ModemFlowControl {
public:
    void LoadDefaults();
    bool SelectAmpK(unsigned k);
    bool ExecuteIfc(const std::string& body, size_t& i, std::string& info);

    void EnterDataState();
    bool TakeGuestData(const uint8_t* data, size_t n, std::vector<uint8_t>& passed);
    bool ReceiveReady(bool rts) const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    static constexpr uint8_t kDceByDteNone        = 0u;
    static constexpr uint8_t kDceByDteXonXoff     = 1u;
    static constexpr uint8_t kDceByDteRts         = 2u;
    static constexpr uint8_t kDceByDteXonXoffPass = 3u;
    static constexpr uint8_t kDteByDceNone        = 0u;
    static constexpr uint8_t kDteByDceXonXoff     = 1u;
    static constexpr uint8_t kDteByDceCts         = 2u;

    void Select(uint8_t dce_by_dte, uint8_t dte_by_dce);
    bool SoftwareFlow() const;

    uint8_t dce_by_dte_ = kDceByDteRts;
    uint8_t dte_by_dce_ = kDteByDceCts;
    bool    xoff_       = false;
};

}
