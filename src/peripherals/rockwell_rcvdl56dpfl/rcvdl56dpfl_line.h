#pragma once

#include "rcvdl56dpfl_configuration.h"
#include "rcvdl56dpfl_receiver.h"
#include "rcvdl56dpfl_transmitter.h"

#include <cstdint>

namespace cerfcore {

class Rcvdl56dpflDspRam;
class StateReader;
class StateWriter;

class Rcvdl56dpflLine {
public:
    using Registers = Rcvdl56dpflConfiguration::Registers;

    explicit Rcvdl56dpflLine(const Rcvdl56dpflDspRam& ram) : ram_(&ram) {}

    Registers&       Regs() { return regs_; }
    const Registers& Regs() const { return regs_; }
    uint8_t          ActiveConf() const { return active_conf_; }

    void Reset(const Registers& defaults);
    void LatchConfiguration();
    bool CarrierWanted() const;
    Rcvdl56dpflLineConfig LineConfig() const;

    void CatchUp(uint64_t elapsed, uint64_t now);
    void RefreshLine();
    const char* QueueTransmitByte(uint8_t value);
    void SetTransmitBufferEmpty(bool empty);
    void HostReadReceiveBuffer();

    void SetStatus(uint32_t reg, uint8_t value);
    void NoteNewStatus();
    void UpdateNewStatusInterrupt();
    void UpdateBufferInterrupts();
    bool IrqAsserted() const;
    uint64_t InterruptRiseTick() const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    bool InternalLoop() const;
    uint64_t NextEventTick() const;
    void ProcessDueEvent();
    bool RateChangeDue(uint32_t reg, uint8_t bit, bool renegotiation) const;
    void CompleteRateChange(uint32_t reg, uint8_t bit, bool renegotiation);
    void ApplyTxOut(const Rcvdl56dpflLineConfig& c, const Rcvdl56dpflTxOut& out);
    void PublishLineStatus();
    void UpdateClearToSend();
    void UpdateLoopStatus();
    uint8_t WaitingInterrupts() const;
    uint8_t RaisedInterrupts() const;
    bool SameStateAs(const Rcvdl56dpflLine& other) const;

    const Rcvdl56dpflDspRam* ram_;
    Registers                regs_{};
    Rcvdl56dpflTransmitter   transmitter_;
    Rcvdl56dpflReceiver      receiver_;
    uint64_t now_          = 0u;
    uint8_t  active_conf_  = 0u;
    bool     loop3_        = false;
    bool     v21_sync_     = false;
    bool     cleared_down_ = false;
};

}
