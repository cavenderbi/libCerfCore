#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class CasioCassiopeiaA10CardWiring : public Service {
public:
    using Service::Service;

    struct Lines {
        bool change       = false;
        bool ireq_latch   = false;
        bool ireq_low     = false;
        bool card_present = false;
    };

    virtual bool SupplyOn()                 = 0;
    virtual void Drive(const Lines& lines)  = 0;
};

}
