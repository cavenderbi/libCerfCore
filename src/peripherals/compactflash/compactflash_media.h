#pragma once

#include "compactflash_image.h"
#include "compactflash_task_file.h"

#include <cstdint>
#include <string>

namespace cerfcore {

class CerfEmulator;

class CompactFlashMedia {
public:
    CompactFlashMedia(CerfEmulator& emu, const std::string& image_path);

    bool     IsOpen() const { return image_.IsOpen(); }
    uint64_t TotalSectors() const { return image_.TotalSectors(); }

    CompactFlashAddressFault Check(const CompactFlashTaskFile& tf) const;
    CompactFlashAddressFault Seek(const CompactFlashTaskFile& tf) const;
    CompactFlashAddressFault Read(const CompactFlashTaskFile& tf, uint8_t* out);
    CompactFlashAddressFault Write(const CompactFlashTaskFile& tf, const uint8_t* in, bool verify);
    CompactFlashAddressFault Format(const CompactFlashTaskFile& tf);
    CompactFlashAddressFault Translate(const CompactFlashTaskFile& tf, uint8_t* out);

private:
    void ReadSector(uint64_t lba, uint8_t* out);
    void WriteSector(uint64_t lba, const uint8_t* in);

    CerfEmulator&     emu_;
    CompactFlashImage image_;
};

}
