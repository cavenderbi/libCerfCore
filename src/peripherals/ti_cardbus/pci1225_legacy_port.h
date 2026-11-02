#pragma once

#include "pci1225_config_space.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

class Pci1225SocketFunction;

class Pci1225LegacyPortOwner {
public:
    virtual ~Pci1225LegacyPortOwner() = default;
    [[noreturn]] virtual void RefuseLegacyAccess(const char* access, uint32_t pci_io, unsigned size,
                                                 const char* reason) = 0;
};

class Pci1225LegacyPort {
public:
    Pci1225LegacyPort(Pci1225SharedConfig& shared, std::mutex& mtx, Pci1225LegacyPortOwner& owner)
        : shared_(shared), mtx_(mtx), owner_(owner) {}

    void Attach(Pci1225SocketFunction& socket_a, Pci1225SocketFunction& socket_b);

    bool     ClaimsLocked(uint32_t pci_io, bool io_space_enabled) const;
    uint32_t Read(uint32_t pci_io, unsigned size);
    void     Write(uint32_t pci_io, uint32_t value, unsigned size);

private:
    Pci1225SocketFunction& Target(uint8_t index) const;

    Pci1225SharedConfig&    shared_;
    std::mutex&             mtx_;
    Pci1225LegacyPortOwner& owner_;
    Pci1225SocketFunction*  socket_a_ = nullptr;
    Pci1225SocketFunction*  socket_b_ = nullptr;
};

}
