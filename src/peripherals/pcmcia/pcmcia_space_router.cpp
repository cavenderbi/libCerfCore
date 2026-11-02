#include "pcmcia_space_router.h"

#include <cerfcore/boards/board_context.h>
#include "../../socs/sa11xx/sa1110_id.h"
#include "../../socs/pxa255/pxa255_id.h"
#include "../../socs/sa11xx/sa1100_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include "pcmcia_host_cycles.h"
#include "pcmcia_slot.h"
#include "../peripheral_dispatcher.h"
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr const char* kFloatingTag = "PC Card space";

}

bool PcmciaSpaceRouter::ShouldRegister() {
    const std::string_view soc = emu_.Get<BoardContext>().GetSocId();
    return soc == SocId::Sa1110 || soc == SocId::Pxa255 ||
           soc == SocId::Sa1100;
}

void PcmciaSpaceRouter::OnReady() {
    emu_.Get<PeripheralDispatcher>().Register(this);
}

void PcmciaSpaceRouter::ProvideSockets(PcmciaSlot* socket0,
                                       PcmciaSlot* socket1) {
    sockets_[0] = socket0;
    sockets_[1] = socket1;
}

PcmciaSlot* PcmciaSpaceRouter::Socket(int n) const {
    return (n == 0 || n == 1) ? sockets_[n] : nullptr;
}

void PcmciaSpaceRouter::SaveState(StateWriter& w) {
    for (int i = 0; i < 2; ++i) {
        PcmciaSlot* s = sockets_[i];
        w.BeginFrame(static_cast<uint32_t>(i));
        w.Write<uint8_t>("socket_present", s ? 1u : 0u);
        if (s)
            s->SaveSlotState(w);
        w.EndFrame();
    }
}

void PcmciaSpaceRouter::RestoreState(StateReader& r) {
    for (int i = 0; i < 2; ++i) {
        if (const uint32_t frame = r.EnterFrame(); frame != static_cast<uint32_t>(i))
            r.Reject("PC Card socket frame 0x%X where this build writes socket %d", frame, i);
        uint8_t present = 0;
        r.Read("socket_present", present);
        if ((present != 0u) != (sockets_[i] != nullptr))
            r.Reject("socket %d is %s in the image and %s in this build", i,
                     present ? "wired" : "absent", sockets_[i] ? "wired" : "absent");
        if (sockets_[i])
            sockets_[i]->RestoreSlotState(r);
        r.LeaveFrame();
    }
}

void PcmciaSpaceRouter::PostRestore() {
    for (PcmciaSlot* s : sockets_)
        if (s) s->PostRestoreSlot();
}

PcmciaSlot* PcmciaSpaceRouter::Decode(uint32_t addr, Region* region,
                                      uint32_t* card_offset) const {
    const uint32_t off    = addr - MmioBase();
    const int      socket = (off >> 28) & 1;             /* 256 MB per socket */
    *region      = static_cast<Region>((off >> 26) & 3); /* 64 MB sub-regions */
    *card_offset = off & 0x03FFFFFFu;
    return sockets_[socket];
}

uint8_t PcmciaSpaceRouter::ReadByte(uint32_t addr) {
    Region region;
    uint32_t off;
    PcmciaSlot* slot = Decode(addr, &region, &off);
    if (!slot) return floating_.Read8(kFloatingTag, addr);
    switch (region) {
        case Region::Io:        return slot->ReadIo8(off);
        case Region::Attribute: return slot->ReadAttribute8(off);
        case Region::Common:    return slot->ReadCommon8(off);
        case Region::Reserved:  break;
    }
    return floating_.Read8(kFloatingTag, addr);
}

uint16_t PcmciaSpaceRouter::ReadHalf(uint32_t addr) {
    Region region;
    uint32_t off;
    PcmciaSlot* slot = Decode(addr, &region, &off);
    if (!slot) return floating_.Read16(kFloatingTag, addr);
    switch (region) {
        case Region::Io:        return static_cast<uint16_t>(ReadCardIo(*slot, off, PcmciaIoWidth::kCardIois16, 2u));
        case Region::Attribute: return slot->ReadAttribute16(off);
        case Region::Common:    return slot->ReadCommon16(off);
        case Region::Reserved:  break;
    }
    return floating_.Read16(kFloatingTag, addr);
}

uint32_t PcmciaSpaceRouter::ReadWord(uint32_t addr) {
    /* 16-bit PC Card bus: a 32-bit access is two halfword cycles. */
    const uint32_t lo = ReadHalf(addr);
    const uint32_t hi = ReadHalf(addr + 2u);
    return lo | (hi << 16);
}

void PcmciaSpaceRouter::WriteByte(uint32_t addr, uint8_t value) {
    Region region;
    uint32_t off;
    PcmciaSlot* slot = Decode(addr, &region, &off);
    if (!slot) {
        floating_.Write(kFloatingTag, 8u, addr);
        return;
    }
    switch (region) {
        case Region::Io:        slot->WriteIo8(off, value);        return;
        case Region::Attribute: slot->WriteAttribute8(off, value); return;
        case Region::Common:    slot->WriteCommon8(off, value);    return;
        case Region::Reserved:  break;
    }
    floating_.Write(kFloatingTag, 8u, addr);
}

void PcmciaSpaceRouter::WriteHalf(uint32_t addr, uint16_t value) {
    Region region;
    uint32_t off;
    PcmciaSlot* slot = Decode(addr, &region, &off);
    if (!slot) {
        floating_.Write(kFloatingTag, 16u, addr);
        return;
    }
    switch (region) {
        case Region::Io:
            WriteCardIo(*slot, off, PcmciaIoWidth::kCardIois16, 2u, value);
            return;
        case Region::Attribute:
            slot->WriteAttribute16(off, value);
            return;
        case Region::Common:
            slot->WriteCommon16(off, value);
            return;
        case Region::Reserved:  break;
    }
    floating_.Write(kFloatingTag, 16u, addr);
}

void PcmciaSpaceRouter::WriteWord(uint32_t addr, uint32_t value) {
    WriteHalf(addr,      static_cast<uint16_t>(value & 0xFFFFu));
    WriteHalf(addr + 2u, static_cast<uint16_t>(value >> 16));
}

REGISTER_SERVICE(PcmciaSpaceRouter);

}
