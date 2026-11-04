#pragma once

#include <cerfcore/core/service.h>
#include "../vr41xx/vr41xx_bcu_boot_write.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace cerfcore {

struct Vr4121DramWiring {
    bool                    dbus32 = false;
    std::array<uint32_t, 4> bank_chip_bytes{};
};

struct Vr4121Revision {
    uint8_t major = 0;
    uint8_t minor = 0;
};

class Vr4121BcuBoard : public Service {
public:
    using Service::Service;

    virtual bool Sdram() const = 0;

    virtual std::optional<Vr4121DramWiring> DramWiring() const = 0;

    virtual std::vector<Vr41xxBcuBootWrite> KernelEntryWrites() const = 0;

    virtual std::optional<Vr4121Revision> Revision() const = 0;
};

}
