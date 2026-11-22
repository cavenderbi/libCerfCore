#include "../../socs/sh77xx/sh77xx_pcmcia_card_bus.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../peripherals/pcmcia/pcmcia_host_cycles.h"
#include "hp_jornada_540_cf_socket.h"
#include "hp_jornada_540_family_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kCfArea          = 6u;
constexpr uint32_t kCardAddressMask = 0x000007FFu;
constexpr uint32_t kRegisterSelect  = 0x00001800u;

class HpJornada540CfCardBus : public Sh77xxPcmciaCardBus {
public:
    using Sh77xxPcmciaCardBus::Sh77xxPcmciaCardBus;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::HpJornada540;
    }

    void OnReady() override { socket_ = &emu_.Get<HpJornada540CfSocket>(); }

    bool ServesArea(uint32_t area) const override { return area == kCfArea; }

    uint32_t ReadCycles(uint32_t area, uint32_t offset, bool io, PcmciaIoWidth width, unsigned bytes) override {
        const uint32_t card = offset & kCardAddressMask;
        if (io) return ReadCardIo(socket_->Slot(), card, width, bytes);
        return ReadCardMemory(socket_->Slot(), card, Attribute(area, offset, "read", 0u),
                              width != PcmciaIoWidth::kEightBit, bytes);
    }

    void WriteCycles(uint32_t area, uint32_t offset, bool io, PcmciaIoWidth width, unsigned bytes,
                     uint32_t value) override {
        const uint32_t card = offset & kCardAddressMask;
        if (io) {
            WriteCardIo(socket_->Slot(), card, width, bytes, value);
            return;
        }
        WriteCardMemory(socket_->Slot(), card, Attribute(area, offset, "write", value),
                        width != PcmciaIoWidth::kEightBit, bytes, value);
    }

private:
    bool Attribute(uint32_t area, uint32_t offset, const char* access, uint32_t value) {
        const uint32_t select = offset & kRegisterSelect;
        if (select == kRegisterSelect) return true;
        if (select == 0u) return false;
        emu_.Get<Fatal>().Die("HpJornada540CfCardBus: memory %s 0x%08X with A11 != A12, area %u offset 0x%08X",
                              access, value, area, offset);
    }

    HpJornada540CfSocket* socket_ = nullptr;
};

}

REGISTER_SERVICE_AS(HpJornada540CfCardBus, Sh77xxPcmciaCardBus);

}
