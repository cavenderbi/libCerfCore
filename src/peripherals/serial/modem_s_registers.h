#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cerfcore {

class StateReader;
class StateWriter;

class ModemSRegisters {
public:
    static constexpr size_t kCount      = 39u;
    static constexpr size_t kEscape     = 2u;
    static constexpr size_t kTerminator = 3u;
    static constexpr size_t kFormat     = 4u;
    static constexpr size_t kEdit       = 5u;
    static constexpr size_t kGuard      = 12u;
    static constexpr size_t kDtrDetect  = 25u;
    static constexpr size_t kLineSpeed  = 37u;

    void    LoadDefaults();
    uint8_t operator[](size_t reg) const { return s_[reg]; }
    bool    Execute(const std::string& body, size_t& i, std::string& info);

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    std::array<uint8_t, kCount> s_{};
};

}
