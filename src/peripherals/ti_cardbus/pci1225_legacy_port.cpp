#include "pci1225_legacy_port.h"

#include "pci1225_socket_function.h"

namespace cerfcore {

namespace {

constexpr uint32_t kIndexPort      = 0u;
constexpr uint32_t kDataPort       = 1u;
constexpr unsigned kPortWidth      = 1u;
constexpr uint8_t  kIndexSocketB   = 0x40u;
constexpr uint8_t  kIndexDevice1   = 0x80u;
constexpr uint8_t  kIndexRegister  = 0x3Fu;
constexpr uint32_t kByteMask       = 0xFFu;

constexpr const char* kWideAccess   = "is wider than one byte";
constexpr const char* kIndexAbove7F = "selects an index above 7Fh";

}

void Pci1225LegacyPort::Attach(Pci1225SocketFunction& socket_a, Pci1225SocketFunction& socket_b) {
    socket_a_ = &socket_a;
    socket_b_ = &socket_b;
}

bool Pci1225LegacyPort::ClaimsLocked(uint32_t pci_io, bool io_space_enabled) const {
    const uint32_t base = shared_.LegacyBase();
    return io_space_enabled && base != 0u && pci_io >= base && pci_io <= base + kDataPort;
}

Pci1225SocketFunction& Pci1225LegacyPort::Target(uint8_t index) const {
    return (index & kIndexSocketB) != 0u ? *socket_b_ : *socket_a_;
}

uint32_t Pci1225LegacyPort::Read(uint32_t pci_io, unsigned size) {
    if (size != kPortWidth)
        owner_.RefuseLegacyAccess("read", pci_io, size, kWideAccess);
    uint32_t port = 0u;
    uint8_t index = 0u;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        port = pci_io - shared_.LegacyBase();
        index = shared_.legacy_index;
    }
    if (port == kIndexPort)
        return index;
    if ((index & kIndexDevice1) != 0u)
        owner_.RefuseLegacyAccess("read", pci_io, size, kIndexAbove7F);
    return Target(index).ExCaRead(index & kIndexRegister, kPortWidth) & kByteMask;
}

void Pci1225LegacyPort::Write(uint32_t pci_io, uint32_t value, unsigned size) {
    if (size != kPortWidth)
        owner_.RefuseLegacyAccess("write", pci_io, size, kWideAccess);
    uint8_t index = 0u;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (pci_io - shared_.LegacyBase() == kIndexPort) {
            shared_.legacy_index = static_cast<uint8_t>(value & kByteMask);
            return;
        }
        index = shared_.legacy_index;
    }
    if ((index & kIndexDevice1) != 0u)
        owner_.RefuseLegacyAccess("write", pci_io, size, kIndexAbove7F);
    Target(index).ExCaWrite(index & kIndexRegister, value & kByteMask, kPortWidth);
}

}
