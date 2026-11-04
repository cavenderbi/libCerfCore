#pragma once

#include "../../socs/uart_endpoint.h"
#include "../ps2_mouse/ps2_mouse.h"
#include "../serial/serial_line.h"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>

namespace cerfcore {

class StateWriter;
class StateReader;

class IbmTrackpoint : public UartEndpoint {
public:
    using HostFormatFn = std::function<SerialLine::LineConfig()>;
    using SendFn       = std::function<void(uint8_t data, const SerialLine::LineConfig& format)>;
    using HaltFn       = std::function<void(const char* what, unsigned value)>;

    IbmTrackpoint(HostFormatFn host_format, SendFn send, HaltFn halt);

    void PowerOn();
    void SetLines(bool dtr_high, bool rts_high);

    void OnGuestTx(uint8_t byte) override;
    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

private:
    enum class Mode : uint8_t { kSerial, kPs2 };
    enum class Parse : uint8_t { kCommand, kE2Sub, kE2Arg1, kE2Arg2 };

    SerialLine::LineConfig Format() const;

    void    SerialMouseReset();
    void    OnByte(uint8_t b);
    void    OnCommand(uint8_t b);
    void    OnE2Sub(uint8_t b);
    void    OnE2Arg1(uint8_t b);
    void    OnE2Arg2(uint8_t b);
    void    Reject();
    void    Flush();
    uint8_t ReadRam(uint8_t addr);

    HostFormatFn host_format_;
    SendFn       send_;
    HaltFn       halt_;
    Ps2Mouse     ps2_;

    Mode                     mode_       = Mode::kSerial;
    uint8_t                  baud_param_ = 0;
    std::array<uint8_t, 256> ram_{};
    std::array<uint8_t, 256> ram_known_{};
    Parse                    parse_          = Parse::kCommand;
    uint8_t                  e2_command_     = 0;
    uint8_t                  e2_arg1_        = 0;
    std::deque<uint8_t>      tx_queue_;
    bool                     id_in_progress_ = false;
    bool                     pending_ps2_    = false;
    uint8_t                  pending_param_  = 0;
    bool                     dtr_high_       = true;
    bool                     rts_high_       = true;
};

}
