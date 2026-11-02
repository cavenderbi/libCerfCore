#pragma once

#include "../peripherals/ricoh_rf5c396/ricoh_rf5c396_board.h"

#include <cstdint>

namespace cerfcore {

class NecVr41xxIsaRf5c396Board : public RicohRf5c396Board {
public:
    using RicohRf5c396Board::RicohRf5c396Board;

    uint32_t IsaIoBase() const override;
    uint32_t IsaMemBase() const override;
    uint8_t  WiredIrqSelect() const override;
    bool     GpiWiredToVs1(int bank) const override;
    void     SetIrqLevel(bool level) override;
};

}
