#include "casio_cassiopeia_a10_chip_register.h"

#include "casio_cassiopeia_a10_key_matrix.h"
#include "casio_cassiopeia_a10_nmi_line.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRegister       = 0x1000000Au;
constexpr uint16_t kOnKeyDown      = 0x0001u;
constexpr uint16_t kOutsideKeyDown = 0x0004u;

class CasioCassiopeiaA10Register00A : public CasioCassiopeiaA10ChipWord<kRegister> {
public:
    using CasioCassiopeiaA10ChipWord::CasioCassiopeiaA10ChipWord;

    void OnReady() override {
        nmi_line_   = &emu_.Get<CasioCassiopeiaA10NmiLine>();
        key_matrix_ = &emu_.Get<CasioCassiopeiaA10KeyMatrix>();
        CasioCassiopeiaA10ChipWord::OnReady();
    }

    uint16_t ReadHalf(uint32_t) override {
        uint16_t value = nmi_line_->OnKeyDown() ? kOnKeyDown : 0u;
        if (key_matrix_->OutsideKeyDown()) value = static_cast<uint16_t>(value | kOutsideKeyDown);
        return value;
    }

    void WriteHalf(uint32_t, uint16_t) override {}

private:
    CasioCassiopeiaA10NmiLine*   nmi_line_   = nullptr;
    CasioCassiopeiaA10KeyMatrix* key_matrix_ = nullptr;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10Register00A);

}
