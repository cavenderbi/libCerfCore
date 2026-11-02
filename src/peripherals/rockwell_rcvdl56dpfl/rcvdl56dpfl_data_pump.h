#pragma once

#include "rcvdl56dpfl_configuration.h"
#include "rcvdl56dpfl_data_pump_host.h"
#include "rcvdl56dpfl_dsp_ram.h"
#include "rcvdl56dpfl_line.h"

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

class Rcvdl56dpflDataPump {
public:
    explicit Rcvdl56dpflDataPump(Rcvdl56dpflDataPumpHost& host);

    uint8_t     Read(uint32_t reg);
    const char* Write(uint32_t reg, uint8_t value);

    void SetResetPin(bool high);
    void TimerElapsed();
    void RearmTimer();

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    void PowerOn();
    void PublishIrq();
    void CatchUp();
    void SyncBitRate();
    const char* WriteRegister(uint32_t reg, uint8_t value);
    const char* WriteNewStatusRegister(uint8_t value);
    void WriteBufferStatusRegister(uint8_t value);
    const char* WriteMemoryAccessRegister(uint8_t value);
    const char* WriteTransmitBuffer(uint8_t value);
    void WriteDownloadControl(uint8_t value);
    void WriteModeRegister(uint8_t value);
    const char* ProcessNewConfiguration();
    void ProcessRamAccess();
    uint32_t RamCode() const;

    Rcvdl56dpflDataPumpHost& host_;
    Rcvdl56dpflDspRam        ram_;
    Rcvdl56dpflLine          line_{ram_};
    bool reset_held_  = false;
    bool downloading_ = false;
    bool irq_level_   = false;
};

}
