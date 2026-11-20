#include "../sh77xx_port_key_matrix.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "hp_300_family_id.h"

#include <iterator>

namespace cerfcore {

namespace {

class Hp300KeyMatrix : public Sh77xxPortKeyMatrix {
public:
    using Sh77xxPortKeyMatrix::Sh77xxPortKeyMatrix;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp300;
    }

    void OnReady() override {
        Sh77xxPortKeyMatrix::OnReady();
        emu_.Get<GuestDeepSleep>().RegisterUserWakeInput([this] {
            SetKey(kOnKeyCell, true);
            SetKey(kOnKeyCell, false);
        });
    }

protected:
    std::span<const PortPin, kRows> RowPins() const override { return kRowPins; }

    std::span<const PortPin> ColumnPins() const override { return kColumnPins; }

    uint16_t SenseColumns() const override { return static_cast<uint16_t>((1u << std::size(kColumnPins)) - 1u); }

    std::span<const ColumnInput> ColumnInputs() const override { return kColumnInputs; }

private:
    static constexpr uint32_t kOnKeyRow    = 7u;
    static constexpr uint32_t kOnKeyColumn = 1u;
    static constexpr uint32_t kOnKeyCell   = kOnKeyRow * kColumns + kOnKeyColumn;

    static constexpr ColumnInput kColumnInputs[] = {{kOnKeyColumn, Sh77xxIntcPin::kIrq0}};

    static constexpr PortPin kRowPins[kRows] = {
        {kSh77xxPortB, 3u}, {kSh77xxPortB, 4u}, {kSh77xxPortB, 5u},  {kSh77xxPortB, 6u},
        {kSh77xxPortB, 7u}, {kSh77xxPortJ, 4u}, {kSh77xxPortSc, 3u}, {kSh77xxPortD, 7u},
    };

    static constexpr PortPin kColumnPins[] = {
        {kSh77xxPortA, 0u}, {kSh77xxPortA, 1u}, {kSh77xxPortA, 2u}, {kSh77xxPortA, 3u},
        {kSh77xxPortA, 4u}, {kSh77xxPortA, 5u}, {kSh77xxPortA, 6u}, {kSh77xxPortA, 7u},
        {kSh77xxPortB, 0u}, {kSh77xxPortB, 1u}, {kSh77xxPortB, 2u},
    };
};

}

REGISTER_SERVICE_AS(Hp300KeyMatrix, Sh77xxPortKeyMatrix);

}
