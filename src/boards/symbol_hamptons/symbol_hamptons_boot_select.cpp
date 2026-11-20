#include "../../socs/pxa27x/pxa27x_boot_select.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "symbol_hamptons_family_id.h"

namespace cerfcore {

namespace {

class SymbolHamptonsBootSelect : public Pxa27xBootSelect {
public:
    using Pxa27xBootSelect::Pxa27xBootSelect;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SymbolHamptons;
    }

    bool BootsFrom16BitMemory() const override { return false; }
};

}

REGISTER_SERVICE_AS(SymbolHamptonsBootSelect, Pxa27xBootSelect);

}
