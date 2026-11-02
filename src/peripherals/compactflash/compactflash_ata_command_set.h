#pragma once

#include <cstdint>

namespace cerfcore {

class CompactFlashAtaCore;

class CompactFlashAtaCommandSet {
public:
    explicit CompactFlashAtaCommandSet(CompactFlashAtaCore& core) : core_(core) {}

    void Execute(uint8_t command, bool was_sleeping);
    void OnReadBufferEnd();
    void OnWriteBufferFull();

private:
    bool ExecutePowerCommand(uint8_t command, bool was_sleeping);
    bool ExecuteSetupCommand(uint8_t command);
    void ExecuteDataCommand(uint8_t command);

    void ReadSectors();
    void LoadReadSector();
    void ReadLong();
    void WalkSectors();
    void Identify();
    void TranslateSector();
    void RequestSense();
    void InitializeParameters();
    void SetMultiple();
    void SetFeatures();
    void WriteSector(bool verify);
    void FinishWithInterrupt();

    CompactFlashAtaCore& core_;
};

}
