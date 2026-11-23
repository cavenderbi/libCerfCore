#include "../amd_command_set_flash.h"

#include "../../core/board_parts.h"
#include "amd_am29lv800bb_part_id.h"
#include <cerfcore/core/cerf_emulator.h>

#include <cstdint>
#include <string>

namespace cerfcore {

namespace {

constexpr uint32_t kBase = 0x00000000u;
constexpr uint32_t kSize = 0x06000000u;  /* 96 MB, matches OAT NOR bank */

/* Autoselect identity: manufacturer 0x01 (AMD), device 0x225B (AM29LV800BB). */
constexpr uint32_t kAutoSelIdent = 0x225B0001u;

/* AMD addresses, 16-bit-bus byte-addressed (word << 1). */
constexpr uint32_t kUnlockAddr1 = 0x5555u << 1;  /* 0xAAAA */
constexpr uint32_t kUnlockAddr2 = 0x2AAAu << 1;  /* 0x5554 */

class Am29Lv800Bb : public AmdCommandSetFlash {
public:
    using AmdCommandSetFlash::AmdCommandSetFlash;

    bool ShouldRegister() override {
        return emu_.Get<BoardParts>().Has(PartId::AmdAm29lv800bb);
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    static constexpr char kObjectName[] = "AMD AM29LV800BB";
    std::string ObjectName() const override { return kObjectName; }

protected:
    uint32_t AutoSelectIdent() const override { return kAutoSelIdent; }
    uint32_t UnlockAddr1()     const override { return kUnlockAddr1; }
    uint32_t UnlockAddr2()     const override { return kUnlockAddr2; }

    /* AM29LV800BB bottom-boot geometry: 16 KB boot block, two 8 KB blocks, one
       32 KB block, then uniform 64 KB sectors. */
    uint32_t SectorSize(uint32_t io_addr) const override {
        if (io_addr < 16u * 1024u) return 16u * 1024u;
        if (io_addr < 24u * 1024u) return  8u * 1024u;
        if (io_addr < 32u * 1024u) return  8u * 1024u;
        if (io_addr < 64u * 1024u) return 32u * 1024u;
        return 64u * 1024u;
    }
};

}  /* namespace */

REGISTER_SERVICE(Am29Lv800Bb);

}
