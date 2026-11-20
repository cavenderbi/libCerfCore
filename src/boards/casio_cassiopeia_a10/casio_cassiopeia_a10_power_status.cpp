#include "casio_cassiopeia_a10_chip_register.h"

#include "casio_cassiopeia_a10_battery.h"
#include "casio_cassiopeia_a10_vdet1r_line.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kStatus = 0x10000004u;

constexpr uint16_t kVdet1    = 0x0002u;
constexpr uint16_t kVdet2    = 0x0004u;
constexpr uint16_t kVdet3    = 0x0008u;
constexpr uint16_t kVdet2r   = 0x0010u;
constexpr uint16_t kRecharge = 0x0020u;
constexpr uint16_t kCradle   = 0x0040u;

class CasioCassiopeiaA10PowerStatus : public CasioCassiopeiaA10ChipWord<kStatus> {
public:
    using CasioCassiopeiaA10ChipWord::CasioCassiopeiaA10ChipWord;

    void OnReady() override {
        battery_ = &emu_.Get<CasioCassiopeiaA10Battery>();
        vdet1r_  = &emu_.Get<CasioCassiopeiaA10Vdet1rLine>();
        CasioCassiopeiaA10ChipWord::OnReady();
    }

    uint16_t ReadHalf(uint32_t) override { return Status(); }

private:
    uint16_t Status() const {
        uint16_t bits = vdet1r_->StatusBits();
        if (battery_->Tripped(CasioCassiopeiaA10Vdet::kVdet1)) bits = static_cast<uint16_t>(bits | kVdet1);
        if (battery_->Tripped(CasioCassiopeiaA10Vdet::kVdet2)) bits = static_cast<uint16_t>(bits | kVdet2 | kVdet2r);
        if (battery_->Tripped(CasioCassiopeiaA10Vdet::kVdet3)) bits = static_cast<uint16_t>(bits | kVdet3);
        if (battery_->Recharging()) bits = static_cast<uint16_t>(bits | kRecharge);
        if (battery_->OnExternalPower()) bits = static_cast<uint16_t>(bits | kCradle);
        return bits;
    }

    CasioCassiopeiaA10Battery*    battery_ = nullptr;
    CasioCassiopeiaA10Vdet1rLine* vdet1r_  = nullptr;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10PowerStatus);

}
