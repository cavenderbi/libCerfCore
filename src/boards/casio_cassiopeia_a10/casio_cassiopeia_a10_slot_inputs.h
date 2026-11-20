#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cstdint>

namespace cerfcore {

class CasioCassiopeiaA10Battery;
class CasioCassiopeiaA10PcCard;

inline constexpr uint32_t kCasioCassiopeiaA10SlotInputs  = 0x10000260u;
inline constexpr uint16_t kCasioCassiopeiaA10CardReleased = 0x0001u;
inline constexpr uint16_t kCasioCassiopeiaA10AdapterInUse = 0x0002u;

class CasioCassiopeiaA10SlotInputs : public CasioCassiopeiaA10ChipWord<kCasioCassiopeiaA10SlotInputs> {
public:
    using CasioCassiopeiaA10ChipWord::CasioCassiopeiaA10ChipWord;

    void OnReady() override;

    uint16_t ReadHalf(uint32_t addr) override;

    uint16_t Inputs() const;

private:
    CasioCassiopeiaA10Battery* battery_ = nullptr;
    CasioCassiopeiaA10PcCard*  pc_card_ = nullptr;
};

}
