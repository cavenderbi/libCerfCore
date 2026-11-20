#pragma once

#include <cerfcore/boot/rom_parser_service.h>
#include <cerfcore/core/service.h>

#include <cstdint>
#include <span>

namespace cerfcore {

class GuestXipImage : public Service {
public:
    using Service::Service;

    virtual const ParsedXipRegion* Primary() = 0;
    virtual const ParsedTOCentry*  FindModule(const char* name) = 0;
    virtual bool KernelSubsystemVersion(uint16_t& major, uint16_t& minor) = 0;
    virtual void Patch(uint32_t pa, std::span<const uint8_t> bytes) = 0;

    void PatchWord(uint32_t pa, uint32_t value);
    void RestoreOnEveryReset(uint32_t pa, uint32_t length);

protected:
    void PatchRam(uint32_t pa, std::span<const uint8_t> bytes);
};

}
