#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

inline constexpr uint32_t kIbmWorkpadZ50RomSpaceEndPa = 0x20000000u;

class IbmWorkpadZ50MemberStraps : public Service {
public:
    using Service::Service;

    virtual uint8_t  KeyboardId() const   = 0;
    virtual bool     ModemFitted() const  = 0;
    virtual uint32_t RomCardBytes() const = 0;

    uint32_t RomCardPa() const { return kIbmWorkpadZ50RomSpaceEndPa - RomCardBytes(); }
};

template <uint8_t kKeyboardId, bool kModemFitted, uint32_t kRomCardBytes>
class IbmWorkpadZ50FixedMemberStraps : public IbmWorkpadZ50MemberStraps {
public:
    using IbmWorkpadZ50MemberStraps::IbmWorkpadZ50MemberStraps;

    uint8_t  KeyboardId() const override   { return kKeyboardId; }
    bool     ModemFitted() const override  { return kModemFitted; }
    uint32_t RomCardBytes() const override { return kRomCardBytes; }
};

inline constexpr uint32_t kIbmWorkpadZ50RomCard20Mb = 0x01400000u;
inline constexpr uint32_t kIbmWorkpadZ50RomCard32Mb = 0x02000000u;

}
