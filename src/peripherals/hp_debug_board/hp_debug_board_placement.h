#pragma once

#include <cerfcore/core/service.h>
#include "../../boards/hp_300/hp_300_family_id.h"
#include "../../boards/hp_600/hp_600_family_id.h"
#include "../../boards/hp_jornada_540/hp_jornada_540_family_id.h"
#include "../../boards/hp_jornada_700/hp_jornada_700_family_id.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

class HpDebugBoardPlacement : public Service {
public:
    using Service::Service;

    static bool FittedOnFamily(std::string_view family) {
        return family == BoardFamilyId::Hp300 || family == BoardFamilyId::Hp600 ||
               family == BoardFamilyId::HpJornada540 || family == BoardFamilyId::HpJornada700;
    }

    virtual uint32_t BasePa() const              = 0;
    virtual uint32_t Cd1284RegisterShift() const = 0;
    virtual uint32_t PostLatchOffset() const     = 0;
};

}
