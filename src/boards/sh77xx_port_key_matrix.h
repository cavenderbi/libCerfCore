#pragma once

#include <cerfcore/core/service.h>
#include "../peripherals/key_matrix_cells.h"
#include "../socs/sh77xx/sh77xx_intc.h"

#include <cstdint>
#include <mutex>
#include <span>

namespace cerfcore {

class Sh77xxPortData;

class Sh77xxPortKeyMatrix : public Service {
public:
    using Service::Service;

    static constexpr uint32_t kRows    = 8u;
    static constexpr uint32_t kColumns = KeyMatrixCells<kRows>::kColumns;
    static constexpr uint32_t kCells   = kRows * kColumns;

    void OnReady() override;

    void SetKey(uint32_t cell, bool down);
    void SetColumnPull(uint16_t columns_low);
    void TrackRows(const Sh77xxPortData& ports);

protected:
    struct PortPin {
        uint32_t port;
        uint32_t pin;
    };

    struct ColumnInput {
        uint32_t      column;
        Sh77xxIntcPin pin;
    };

    virtual std::span<const PortPin, kRows> RowPins() const = 0;
    virtual std::span<const PortPin> ColumnPins() const = 0;
    virtual uint16_t SenseColumns() const = 0;
    virtual std::span<const ColumnInput> ColumnInputs() const = 0;

private:
    void PublishLocked(const Sh77xxPortData& ports);

    std::mutex             mtx_;
    Sh77xxIntc*            intc_          = nullptr;
    uint16_t               sense_columns_ = 0u;
    KeyMatrixCells<kRows>  cells_;
    uint16_t               pulled_low_    = 0u;
    uint8_t                rows_low_      = 0u;
};

}
