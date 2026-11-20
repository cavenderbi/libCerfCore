#include "../sh77xx_port_key_matrix.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "hp_600_family_id.h"
#include "hp_600_keyboard_layout.h"

namespace cerfcore {

namespace {

class Hp600KeyMatrix : public Sh77xxPortKeyMatrix {
public:
    using Sh77xxPortKeyMatrix::Sh77xxPortKeyMatrix;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

protected:
    std::span<const PortPin, kRows> RowPins() const override { return kRowPins; }

    std::span<const PortPin> ColumnPins() const override { return kColumnPins; }

    uint16_t SenseColumns() const override {
        uint16_t columns = 0u;
        for (uint16_t scanned : emu_.Get<Hp600KeyboardLayout>().ScannedColumns()) {
            columns = static_cast<uint16_t>(columns | scanned);
        }
        return columns;
    }

    std::span<const ColumnInput> ColumnInputs() const override { return {}; }

private:
    static constexpr PortPin kRowPins[kRows] = {
        {kSh77xxPortD, 1u}, {kSh77xxPortD, 5u}, {kSh77xxPortD, 7u}, {kSh77xxPortE, 0u},
        {kSh77xxPortE, 1u}, {kSh77xxPortE, 3u}, {kSh77xxPortE, 6u}, {kSh77xxPortE, 7u},
    };

    static constexpr PortPin kColumnPins[kColumns] = {
        {kSh77xxPortF, 0u}, {kSh77xxPortF, 1u}, {kSh77xxPortF, 2u}, {kSh77xxPortF, 3u},
        {kSh77xxPortF, 4u}, {kSh77xxPortF, 5u}, {kSh77xxPortF, 6u}, {kSh77xxPortF, 7u},
        {kSh77xxPortC, 0u}, {kSh77xxPortC, 1u}, {kSh77xxPortC, 2u}, {kSh77xxPortC, 3u},
        {kSh77xxPortC, 4u}, {kSh77xxPortC, 5u}, {kSh77xxPortC, 6u}, {kSh77xxPortC, 7u},
    };
};

}

REGISTER_SERVICE_AS(Hp600KeyMatrix, Sh77xxPortKeyMatrix);

}
