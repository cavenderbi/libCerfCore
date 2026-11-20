#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/key_matrix_cells.h"

#include <cstdint>

namespace cerfcore {

class CasioCassiopeiaA10OnKey;
class CasioCassiopeiaA10RequestLines;

class CasioCassiopeiaA10KeyMatrix : public Peripheral {
public:
    static constexpr uint32_t kRows        = 9u;
    static constexpr uint32_t kColumns     = KeyMatrixCells<kRows>::kColumns;
    static constexpr uint32_t kOutsideKey  = kRows * kColumns;
    static constexpr uint32_t kOnKey       = kOutsideKey + 1u;

    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

    void SetKey(uint32_t code, bool down);
    bool OutsideKeyDown() const { return outside_key_; }

private:
    void SetOutsideKey(bool down);
    void DriveMatrixLine();
    void Reset();

    CasioCassiopeiaA10RequestLines* requests_ = nullptr;
    CasioCassiopeiaA10OnKey*        on_key_   = nullptr;
    KeyMatrixCells<kRows>           cells_;
    bool                        outside_key_ = false;
    uint16_t                    rows_        = 0u;
    uint16_t                    word_e0_     = 0u;
    uint16_t                    word_e2_     = 0u;
    uint16_t                    word_e6_     = 0u;
    uint8_t                     written_     = 0u;
};

}
