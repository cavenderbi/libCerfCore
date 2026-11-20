#include "../sh77xx/sh77xx_cpg_impl.h"
#include "hd6417096_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kPllenPstbyStcIfcPfc = 0x00FFu;

constexpr Sh77xxFrqcrSetting kHd6417096Settings[] = {
    {0x0100u, 1u, 1u}, {0x0101u, 1u, 2u}, {0x0102u, 1u, 4u}, {0x0110u, 1u, 1u}, {0x0111u, 1u, 2u},
    {0x0112u, 1u, 4u}, {0x0120u, 1u, 1u}, {0x0121u, 1u, 2u}, {0x0122u, 1u, 4u}, {0x0140u, 1u, 1u},
    {0x0141u, 1u, 2u}, {0x0142u, 1u, 4u}, {0x0150u, 1u, 1u}, {0x0151u, 1u, 2u}, {0x0152u, 1u, 4u},
    {0x0160u, 1u, 1u}, {0x0161u, 1u, 2u}, {0x0162u, 1u, 4u}, {0x0180u, 1u, 1u}, {0x0181u, 1u, 2u},
    {0x0182u, 1u, 4u}, {0x0190u, 2u, 1u}, {0x0191u, 2u, 2u}, {0x0194u, 1u, 1u}, {0x0195u, 1u, 2u},
    {0x0196u, 1u, 4u}, {0x01A0u, 4u, 1u}, {0x01A4u, 2u, 1u}, {0x01A5u, 2u, 2u}, {0x01A8u, 1u, 1u},
    {0x01A9u, 1u, 2u}, {0x01AAu, 1u, 4u}, {0x01C0u, 1u, 1u}, {0x01C1u, 1u, 2u}, {0x01C2u, 1u, 4u},
    {0x01D0u, 2u, 1u}, {0x01D1u, 2u, 2u}, {0x01D4u, 1u, 1u}, {0x01D5u, 1u, 2u}, {0x01D6u, 1u, 4u},
    {0x01E0u, 4u, 1u}, {0x01E4u, 2u, 1u}, {0x01E5u, 2u, 2u}, {0x01E8u, 1u, 1u}, {0x01E9u, 1u, 2u},
    {0x01EAu, 1u, 4u},
};

constexpr Sh77xxCpgModel kHd6417096Cpg{
    {kHd6417096Settings, static_cast<uint32_t>(std::size(kHd6417096Settings)), kPllenPstbyStcIfcPfc},
    {nullptr, 0u, 0u},
};

class Hd6417096Cpg : public Sh77xxCpgBase<SocId::Hd6417096, kHd6417096Cpg> {
public:
    using Sh77xxCpgBase::Sh77xxCpgBase;
};

}

REGISTER_SERVICE_AS(Hd6417096Cpg, Sh77xxCpg);

}
