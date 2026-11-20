#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/service.h>
#include "casio_cassiopeia_a10_serial_connector.h"

#include <optional>

namespace cerfcore {

class CasioCassiopeiaA10ConnectorRoute : public Service {
public:
    using Service::Service;

    virtual std::optional<CasioCassiopeiaA10SerialUnit> Routed() const                                   = 0;
    virtual std::optional<CasioCassiopeiaA10SerialUnit> Joined() const                                   = 0;
    virtual void                                        AddChangeListener(CallbackList<>::Callback listener) = 0;
};

}
