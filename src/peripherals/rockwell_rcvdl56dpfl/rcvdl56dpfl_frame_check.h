#pragma once

#include <cstdint>

namespace cerfcore {

class StateReader;
class StateWriter;

class Rcvdl56dpflFrameCheck {
public:
    static constexpr uint8_t kFcs16Bytes = 2u;
    static constexpr uint8_t kFcs32Bytes = 4u;

    void    Start(uint8_t fcs_bytes);
    void    Clear() { *this = Rcvdl56dpflFrameCheck{}; }
    void    Update(uint8_t value);
    uint8_t Bytes() const { return bytes_; }
    uint8_t FcsByte(uint8_t index) const;
    bool    Good() const;

    void SaveState(StateWriter& w, const char* bytes_key, const char* value_key) const;
    void RestoreState(StateReader& r, const char* bytes_key, const char* value_key);

    bool operator==(const Rcvdl56dpflFrameCheck&) const = default;

private:
    uint8_t  bytes_ = 0u;
    uint32_t value_ = 0u;
};

}
