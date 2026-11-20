#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_object.h>

#include <cstdint>
#include <optional>

namespace cerfcore {

struct Pr31x00SerialPin {
    enum class Bank { Mfio, Io };

    Bank bank = Bank::Mfio;
    int  pin  = -1;   /* -1: the board does not wire this line */

    bool Wired() const { return pin >= 0; }

    static Pr31x00SerialPin OnMfio(int p) { return {Bank::Mfio, p}; }
    static Pr31x00SerialPin OnIo(int p)   { return {Bank::Io, p}; }
};

struct Pr31x00SerialModem {
    ObjectIdentity   port;
    Pr31x00SerialPin cts, dcd, dsr, ri;   /* inputs: an endpoint drives these */
    Pr31x00SerialPin dtr, rts;            /* outputs: the guest drives these */
    Pr31x00SerialPin power;
};

class Pr31x00SerialWiring : public Service {
public:
    using Service::Service;

    virtual std::optional<Pr31x00SerialModem> ForUart(uint32_t mmio_base) const = 0;
};

}
