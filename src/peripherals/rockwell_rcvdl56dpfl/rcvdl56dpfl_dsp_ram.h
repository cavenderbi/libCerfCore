#pragma once

#include <array>
#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

class Rcvdl56dpflDspRam {
public:
    static constexpr uint32_t kCodes = 0x2000u;

    void PowerOn();
    void Write(uint32_t code, uint16_t word);
    uint16_t Read(uint32_t code) const;

    void ApplyNewConfiguration(uint8_t conf, bool guard_tone_550, bool compromise_equalizer);
    void ApplyDtrOff();

    bool MemoryAccessNewsMasked() const;
    uint8_t StatusChangeMask(uint32_t reg) const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    uint8_t Byte(uint32_t code) const { return static_cast<uint8_t>(words_[code]); }
    void ResetToPowerOnValue(uint32_t code);

    std::array<uint16_t, kCodes> words_{};
};

}
