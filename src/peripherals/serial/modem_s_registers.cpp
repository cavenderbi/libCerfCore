#include "modem_s_registers.h"

#include "at_decimal.h"

#include <cerfcore/state/state_stream.h>

#include <cstdio>
#include <iterator>

namespace cerfcore {

namespace {

struct SRegisterSpec {
    bool    defined;
    uint8_t min;
    uint8_t max;
    uint8_t def;
};

constexpr SRegisterSpec kSRegisters[] = {
    {true, 0, 255, 0},  {true, 0, 255, 0},  {true, 0, 255, 43}, {true, 0, 127, 13},
    {true, 0, 127, 10}, {true, 0, 127, 8},  {true, 2, 255, 2},  {true, 1, 255, 30},
    {true, 0, 255, 2},  {true, 1, 255, 60}, {true, 1, 255, 14}, {true, 50, 255, 95},
    {true, 0, 255, 50}, {},                 {},                 {},
    {},                 {},                 {true, 0, 255, 0},  {},
    {},                 {},                 {},                 {},
    {},                 {true, 0, 255, 5},  {true, 0, 255, 1},  {},
    {},                 {},                 {},                 {},
    {},                 {},                 {},                 {},
    {true, 0, 1, 1},    {true, 0, 9, 0},    {true, 0, 255, 20},
};
static_assert(std::size(kSRegisters) == ModemSRegisters::kCount);

bool SValueValid(size_t reg, unsigned value) {
    const SRegisterSpec& spec = kSRegisters[reg];
    if (value < spec.min || value > spec.max) return false;
    if (reg == ModemSRegisters::kEdit) return value <= 32u || value == 127u;
    if (reg == ModemSRegisters::kLineSpeed) return value != 4u && value != 8u;
    return true;
}

}

void ModemSRegisters::LoadDefaults() {
    for (size_t reg = 0; reg < kCount; ++reg) s_[reg] = kSRegisters[reg].def;
}

bool ModemSRegisters::Execute(const std::string& body, size_t& i, std::string& info) {
    unsigned     reg    = 0;
    const size_t digits = ParseAtDecimal(body, i, reg);
    if (digits == 0 || reg >= kCount || !kSRegisters[reg].defined) return false;
    i += digits;
    if (i >= body.size()) return false;
    const char op = body[i++];
    if (op == '?') {
        char text[4];
        std::snprintf(text, sizeof text, "%03u", static_cast<unsigned>(s_[reg]));
        info = text;
        return true;
    }
    if (op != '=') return false;
    unsigned     value = 0;
    const size_t n     = ParseAtDecimal(body, i, value);
    if (n == 0 || !SValueValid(reg, value)) return false;
    i += n;
    s_[reg] = static_cast<uint8_t>(value);
    return true;
}

void ModemSRegisters::SaveState(StateWriter& w) const {
    w.WriteBytes("modem_s_registers", s_.data(), s_.size());
}

void ModemSRegisters::RestoreState(StateReader& r) {
    r.ReadBytes("modem_s_registers", s_.data(), s_.size());
}

}
