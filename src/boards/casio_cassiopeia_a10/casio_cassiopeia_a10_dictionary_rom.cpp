#include "../../peripherals/casio_dictionary_rom/casio_dictionary_rom.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_area1.h"
#include "casio_cassiopeia_a10_family_id.h"

#include <string_view>

namespace cerfcore {

namespace {

class CasioCassiopeiaA10DictionaryRom : public CasioDictionaryRomWindow {
public:
    using CasioDictionaryRomWindow::CasioDictionaryRomWindow;

    uint32_t MmioBase() const override { return kSh77xxArea1PaBase; }

protected:
    std::string_view WindowBoardFamilyId() const override { return BoardFamilyId::CasioCassiopeiaA10; }
    const char*      WindowTag()     const override { return "A-51 dictionary ROM"; }
};

}

REGISTER_SERVICE(CasioCassiopeiaA10DictionaryRom);

}
