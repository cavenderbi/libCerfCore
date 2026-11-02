#pragma once

#include "compactflash_data_transfer.h"
#include "compactflash_media.h"
#include "compactflash_power_mode.h"
#include "compactflash_task_file.h"

#include <cstdint>
#include <string>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

class CompactFlashAtaCore {
public:
    CompactFlashAtaCore(CerfEmulator& emu_ref, const std::string& image_path);

    void PowerOn();
    void PowerOff();
    void SoftResetComplete();

    bool InterruptRequest() const;
    void IrqAssert();
    void IrqClear() { intrq = false; }

    void Complete();
    void Fail(uint8_t error_bits, uint8_t sense_code);
    bool Succeeded(CompactFlashAddressFault fault);
    void BeginRead(CompactFlashTransfer kind);
    void BeginWrite(CompactFlashTransfer kind, uint32_t sectors);
    void FinishTransfer();

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

    CerfEmulator&            emu;
    CompactFlashMedia        media;
    CompactFlashTaskFile     tf;
    CompactFlashPowerMode    power;
    CompactFlashDataTransfer xfer;

    uint32_t sectors_left = 0;
    bool     intrq        = false;
    bool     multiple     = false;
    uint8_t  sense        = 0;
    bool     soft_reset_restores_defaults = true;

private:
    uint8_t IdleStatus() const;
};

}
