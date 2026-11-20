#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/key_matrix_cells.h"

#include <cstdint>

namespace cerfcore {

class SharpMobilonHcKeyMatrix : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return kSize; }

    uint16_t ReadHalf(uint32_t addr) override;

    void SetKey(uint8_t col, uint8_t row, bool down);

    void RestoreState(StateReader& r) override;

private:
    static constexpr uint32_t kBase        = 0x6C000000u;
    static constexpr uint32_t kSize        = 0x1000u;
    static constexpr uint32_t kColumnCount = 8u;
    static constexpr uint32_t kAllColumns  = (1u << kColumnCount) - 1u;

    using Cells = KeyMatrixCells<kColumnCount>;

    Cells cells_;
};

}
