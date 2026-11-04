#pragma once

#include <cstdint>

namespace cerfcore {

class VgaRegisterFile;

class VgaStandardRenderer {
public:
    static bool     DisplayEnabled(const VgaRegisterFile& regs);
    static uint32_t Width(const VgaRegisterFile& regs);
    static uint32_t Height(const VgaRegisterFile& regs);
    static uint32_t Depth(const VgaRegisterFile& regs);
    static void     Render(const VgaRegisterFile& regs, uint32_t start, const uint8_t* vram,
                           uint32_t vram_bytes, uint64_t frame, uint32_t* dib, uint32_t dst_w,
                           uint32_t dst_h);
};

}
