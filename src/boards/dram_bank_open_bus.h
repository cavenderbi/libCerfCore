#pragma once

#include "../peripherals/open_bus_window.h"

#include <cstdint>

namespace cerfcore {

class DramBankOpenBus : public OpenBusWindow {
public:
    using OpenBusWindow::OpenBusWindow;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

protected:
    virtual uint32_t BankPaBase() const = 0;
    virtual uint32_t BankBytes() const  = 0;

    bool WindowOnBoard(const BoardContext& bd) const override;

private:
    uint32_t BackedBytes() const;
};

}
