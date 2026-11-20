#include "casio_cassiopeia_a10_chip_register.h"

#include "casio_cassiopeia_a10_chip_intc.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kSourceLevels = 0x10000016u;

class CasioCassiopeiaA10SourceLevels : public CasioCassiopeiaA10ChipWord<kSourceLevels> {
public:
    using CasioCassiopeiaA10ChipWord::CasioCassiopeiaA10ChipWord;

    void OnReady() override {
        chip_intc_ = &emu_.Get<CasioCassiopeiaA10ChipIntc>();
        CasioCassiopeiaA10ChipWord::OnReady();
    }

    uint16_t ReadHalf(uint32_t) override { return chip_intc_->SourceLevels(); }

private:
    CasioCassiopeiaA10ChipIntc* chip_intc_ = nullptr;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10SourceLevels);

}
