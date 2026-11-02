#pragma once

#include "compactflash_ata_command_set.h"
#include "compactflash_ata_core.h"
#include "compactflash_task_file.h"

#include <cstdint>
#include <string>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

class CompactFlashAta {
public:
    CompactFlashAta(CerfEmulator& emu, const std::string& image_path);

    bool     MediaOpen() const { return core_.media.IsOpen(); }
    uint64_t TotalSectors() const { return core_.media.TotalSectors(); }

    void PowerOn();
    void PowerOff();
    void HostAccess() { core_.power.HostAccess(); }
    void SetCardNumber(uint8_t number) { card_number_ = number; }
    void HoldCardReset(bool asserted);
    void SetPowerDown(bool requested);

    const CompactFlashTaskFile& TaskFile() const { return core_.tf; }
    void    WriteTaskFile(uint8_t CompactFlashTaskFile::* field, uint8_t value);
    uint8_t ReadError() const;
    uint8_t ReadDriveAddress() const;

    uint8_t  ReadData8();
    uint8_t  ReadOddData8();
    uint16_t ReadData16();
    void     WriteData8(uint8_t value);
    void     WriteOddData8(uint8_t value);
    void     WriteData16(uint16_t value);

    uint8_t ReadStatus();
    uint8_t AltStatus() const { return SelectedStatus(); }
    void    WriteDeviceControl(uint8_t value);
    void    WriteCommand(uint8_t command);

    bool InterruptRequest() const { return core_.InterruptRequest(); }
    bool Ready() const;

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

private:
    bool    Selected() const { return core_.tf.DriveSelect() == card_number_; }
    bool    Resetting() const { return in_reset_ || in_card_reset_; }
    bool    DataPortIn() const;
    bool    DataPortOut() const;
    uint8_t SelectedStatus() const;
    void    EnterReset();
    void    AfterRead();
    void    AfterWrite();

    CompactFlashAtaCore       core_;
    CompactFlashAtaCommandSet commands_;
    bool                      in_reset_      = false;
    bool                      in_card_reset_ = false;
    uint8_t                   card_number_   = 0;
};

}
