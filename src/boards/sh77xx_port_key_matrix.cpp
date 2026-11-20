#include "sh77xx_port_key_matrix.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/hibernation.h>
#include "../socs/sh77xx/sh77xx_intc.h"
#include "../socs/sh77xx/sh77xx_port_data.h"

namespace cerfcore {

void Sh77xxPortKeyMatrix::OnReady() {
    intc_          = &emu_.Get<Sh77xxIntc>();
    sense_columns_ = SenseColumns();
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] {
        const Sh77xxPortData&       ports = emu_.Get<Sh77xxPortData>();
        std::lock_guard<std::mutex> lk(mtx_);
        cells_.ReleaseAll();
        PublishLocked(ports);
    });
}

void Sh77xxPortKeyMatrix::SetKey(uint32_t cell, bool down) {
    const Sh77xxPortData&       ports = emu_.Get<Sh77xxPortData>();
    std::lock_guard<std::mutex> lk(mtx_);
    cells_.Set(cell, down);
    PublishLocked(ports);
}

void Sh77xxPortKeyMatrix::SetColumnPull(uint16_t columns_low) {
    const Sh77xxPortData&       ports = emu_.Get<Sh77xxPortData>();
    std::lock_guard<std::mutex> lk(mtx_);
    pulled_low_ = columns_low;
    PublishLocked(ports);
}

void Sh77xxPortKeyMatrix::TrackRows(const Sh77xxPortData& ports) {
    const std::span<const PortPin, kRows> rows     = RowPins();
    uint8_t                               rows_low = 0u;
    for (uint32_t row = 0; row < kRows; ++row) {
        if (ports.Drive(rows[row].port, rows[row].pin) == PinDrive::kLow) {
            rows_low = static_cast<uint8_t>(rows_low | (1u << row));
        }
    }
    std::lock_guard<std::mutex> lk(mtx_);
    rows_low_ = rows_low;
    PublishLocked(ports);
}

void Sh77xxPortKeyMatrix::PublishLocked(const Sh77xxPortData& ports) {
    const uint16_t                 low     = static_cast<uint16_t>(pulled_low_ | cells_.Pressed(rows_low_));
    const std::span<const PortPin> columns = ColumnPins();
    for (uint32_t column = 0; column < columns.size(); ++column) {
        if (((sense_columns_ >> column) & 1u) == 0u) continue;
        intc_->SetPinLevel(ports.IntcPinOf(columns[column].port, columns[column].pin), ((low >> column) & 1u) == 0u);
    }
    for (const ColumnInput& input : ColumnInputs()) intc_->SetPinLevel(input.pin, ((low >> input.column) & 1u) == 0u);
}

}
