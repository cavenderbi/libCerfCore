#include "at_decimal.h"

#include <cctype>

namespace cerfcore {

namespace {

constexpr unsigned kValueCap = 255u;

}

size_t ParseAtDecimal(const std::string& text, size_t at, unsigned& value) {
    size_t n = 0;
    value = 0;
    while (at + n < text.size() && isdigit(static_cast<unsigned char>(text[at + n]))) {
        if (value <= kValueCap) value = value * 10u + static_cast<unsigned>(text[at + n] - '0');
        ++n;
    }
    return n;
}

bool ReadAtValue(const std::string& text, size_t& i, unsigned max, unsigned& value) {
    i += ParseAtDecimal(text, i, value);
    return value <= max;
}

}
