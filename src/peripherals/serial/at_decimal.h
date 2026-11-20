#pragma once

#include <cstddef>
#include <string>

namespace cerfcore {

size_t ParseAtDecimal(const std::string& text, size_t at, unsigned& value);
bool   ReadAtValue(const std::string& text, size_t& i, unsigned max, unsigned& value);

}
