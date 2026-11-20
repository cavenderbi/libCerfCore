#include "../sh77xx/sh77xx_cpg_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr uint16_t kPllenPstbyStcIfcPfc = 0x00FFu;
constexpr uint16_t kStcIfcPfc           = 0x003Fu;

constexpr Sh77xxFrqcrSetting kSh7707Modes3467[] = {
    {0x0100u, 1u, 1u}, {0x0101u, 1u, 2u}, {0x0102u, 1u, 4u}, {0x01D0u, 2u, 1u},
    {0x01D1u, 2u, 2u}, {0x01D4u, 1u, 1u}, {0x01D5u, 1u, 2u}, {0x01D6u, 1u, 4u},
    {0x01E0u, 4u, 1u}, {0x01E4u, 2u, 1u}, {0x01E5u, 2u, 2u}, {0x01E8u, 1u, 1u},
    {0x01E9u, 1u, 2u}, {0x01EAu, 1u, 4u},
};

constexpr Sh77xxFrqcrSetting kSh7707Modes012[] = {
    {0x0100u, 1u, 1u}, {0x0101u, 1u, 2u}, {0x0102u, 1u, 4u}, {0x0111u, 2u, 1u},
    {0x0112u, 2u, 2u}, {0x0115u, 1u, 1u}, {0x0116u, 1u, 2u}, {0x0122u, 4u, 1u},
    {0x0126u, 2u, 1u}, {0x012Au, 1u, 1u},
};

constexpr Sh77xxCpgModel kSh7707Cpg{
    {kSh7707Modes3467, static_cast<uint32_t>(std::size(kSh7707Modes3467)), kPllenPstbyStcIfcPfc},
    {kSh7707Modes012, static_cast<uint32_t>(std::size(kSh7707Modes012)), kStcIfcPfc},
};

class Sh7707Cpg : public Sh77xxCpgBase<SocId::Sh7707, kSh7707Cpg> {
public:
    using Sh77xxCpgBase::Sh77xxCpgBase;
};

}

REGISTER_SERVICE_AS(Sh7707Cpg, Sh77xxCpg);

}
