#pragma once

#include <cstddef>
#include <cstdint>

namespace cerfcore {

struct NecBatteryAdcStep {
    uint16_t word;
    int      percent;
};

template <size_t N>
constexpr uint16_t NecBatteryAdcWord(const NecBatteryAdcStep (&table)[N], int fill) {
    for (const NecBatteryAdcStep& step : table) {
        if (fill >= step.percent) return step.word;
    }
    return table[N - 1u].word;
}

}
