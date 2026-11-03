#pragma once

#include "serial_endpoint.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace cerfcore {

class CerfEmulator;
class PppTerminator;

class ModemPersonality : public SerialEndpoint {
public:
    static constexpr size_t kSRegisterCount = 39u;

    ModemPersonality(CerfEmulator& emu, std::string net_id);
    ~ModemPersonality() override;

    void OnGuestTx(const uint8_t* data, size_t n) override;
    void OnControlLines(bool dtr, bool rts) override;
    void OnOpen() override;
    void OnClose() override;
    void ResendModemInputs() override;
    void OnEndpointTimer() override;
    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

private:
    enum class Result { Ok, Connect, NoCarrier, Error };
    enum class Prefix : uint8_t { None, UpperA, LowerA, Body };

    void HandleCommandByte(uint8_t b);
    void ProcessCommandLine();
    void ExecuteBody(const std::string& body);
    bool ExecuteSRegister(const std::string& body, size_t& i);
    void LoadDefaults();
    void Reply(Result result);
    void PushInfo(const char* text);
    void PushStr(const std::string& s);
    void SetCarrier(bool on);
    void EnterDataMode();
    void EnterOnlineCommandState();
    void PushModemInputs();
    void UpdateRts();
    bool OnGuestDataByte(uint8_t b, int64_t now);
    void OnDtrLoss();
    void ArmTimer();
    int64_t NowNs();

    CerfEmulator& emu_;
    std::string   net_id_;
    std::unique_ptr<PppTerminator> terminator_;

    std::array<uint8_t, kSRegisterCount> s_{};
    std::string line_;
    std::string last_body_;
    Prefix      prefix_        = Prefix::None;
    bool        line_overflow_ = false;
    bool        echo_          = true;
    bool        quiet_         = false;
    bool        verbose_       = true;
    uint8_t     dcd_mode_      = 1u;
    uint8_t     dtr_mode_      = 2u;

    bool online_    = false;
    bool data_mode_ = false;
    bool dtr_       = false;
    bool rts_       = false;

    bool    dtr_loss_pending_ = false;
    int64_t dtr_loss_due_     = 0;
    uint8_t escape_count_     = 0u;
    int64_t escape_due_       = 0;
    int64_t last_tx_ns_       = 0;
};

}
