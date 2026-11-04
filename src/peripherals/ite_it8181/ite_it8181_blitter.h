#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cerfcore {

class CerfEmulator;
class IteIt8181Vga;

class IteIt8181Blitter {
public:
    struct Registers {
        uint32_t bsle = 0;
        uint32_t bdls = 0;
        uint32_t bpoa = 0;
        uint32_t bwh  = 0;
        uint32_t bso  = 0;
        uint32_t fcr  = 0;
        uint32_t bcr  = 0;
        uint32_t bc   = 0;
        uint32_t asds = 0;
        uint32_t let  = 0;
        uint32_t lst  = 0;
        uint32_t lsb  = 0;
        uint32_t lsr  = 0;
    };

    IteIt8181Blitter(CerfEmulator& emu, std::vector<uint8_t>& vram, const IteIt8181Vga& vga)
        : emu_(emu), vram_(vram), vga_(vga) {}

    void   CheckCommand(const Registers& r) const;
    size_t HostBytes(const Registers& r) const;
    void   Execute(const Registers& r, const uint8_t* host);

private:
    struct Target {
        uint32_t bytes;
        uint32_t mask;
        uint32_t pitch;
    };

    Target   TargetOf(const Registers& r) const;
    uint32_t PixelAddress(const Target& t, int64_t x, int64_t y) const;
    uint32_t ReadPixel(const Target& t, int64_t x, int64_t y) const;
    void     WritePixel(const Target& t, int64_t x, int64_t y, uint32_t value);
    uint32_t PatternPixel(const Registers& r, const Target& t, int64_t x, int64_t y) const;
    void     Rectangle(const Registers& r, const uint8_t* host);
    void     Line(const Registers& r);

    CerfEmulator&         emu_;
    std::vector<uint8_t>& vram_;
    const IteIt8181Vga&   vga_;
};

}
