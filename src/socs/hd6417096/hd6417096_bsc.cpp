#include "../sh77xx/sh77xx_bsc_impl.h"
#include "hd6417096_id.h"

namespace cerfcore {

namespace {

constexpr sh77xx_bsc::Model kHd6417096Bsc{
    .bcr1_writable               = 0x17FFu,
    .wcr1_writable               = 0x3FFFu,
    .mcr_writable                = 0xFF7Fu,
    .pcr_writable                = 0x00FFu,
    .pcr_write_zero              = 0x0000u,
    .tpc_high_codes_reserved     = false,
    .tpc2_high_codes_reserved    = false,
    .sdram                       = false,
    .bcr2_writable               = 0x3FFDu,
    .bcr2_reset                  = 0x3FFCu,
    .bcr2_port_enable            = 0x0001u,
    .bcr2_first_area             = 1u,
    .area6_pcmcia_size_unchecked = false,
    .wcr1_reset                  = 0x3FFFu,
    .wcr1_write_zero             = 0x8000u,
    .bcr1_reset                  = 0x0000u,
    .bcr1_write_zero             = 0x21E0u,
    .mcr_write_zero              = 0x0300u,
    .dcr_write_zero              = 0x0300u,
    .mcr_dram_longword           = 0x0020u,
    .bcr3                        = false,
    .dramtp_area2_word           = 0x20u,
    .dramtp_area3_word           = 0x20u,
    .area2_dram_limits_all_areas = true,
    .sdram_mode_register         = true,
};

constexpr sh77xx_bsc::RegWindowModel kHd6417096BscWindow = sh77xx_bsc::WindowOf(kHd6417096Bsc);

class Hd6417096Bsc : public sh77xx_bsc::Sh77xxBscBase<SocId::Hd6417096, kHd6417096BscWindow, kHd6417096Bsc> {
public:
    using Sh77xxBscBase::Sh77xxBscBase;
};

}

REGISTER_SERVICE_AS(Hd6417096Bsc, Sh77xxBsc);

}
