#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/boot/rom_parser_service.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/emulated_memory.h>
#include "../page_table_builder.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "ibm_workpad_z50_family_id.h"
#include "ibm_workpad_z50_member_straps.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace cerfcore {

namespace {

constexpr uint32_t kWordBytes    = 4u;
constexpr uint32_t kBlankWord    = 0xFFFFFFFFu;
constexpr uint8_t  kBlankByte    = 0xFFu;
constexpr uint32_t kBootBlockPa  = 0x1FC00000u;
constexpr uint32_t kBootBlockEnd = 0x1FC40000u;

class IbmWorkpadZ50RomCardChecksum : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
    }

    void OnReady() override {
        const auto& straps = emu_.Get<IbmWorkpadZ50MemberStraps>();
        card_pa_    = straps.RomCardPa();
        card_bytes_ = straps.RomCardBytes();
        FillOutsideImage();
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return card_pa_; }
    uint32_t MmioSize() const override { return kWordBytes; }

    uint8_t ReadByte(uint32_t addr) override {
        return static_cast<uint8_t>(Checksum() >> (8u * (addr - card_pa_)));
    }

    uint16_t ReadHalf(uint32_t addr) override {
        return static_cast<uint16_t>(Checksum() >> (8u * (addr - card_pa_)));
    }

    uint32_t ReadWord(uint32_t) override { return Checksum(); }

private:
    void FillOutsideImage() {
        auto& pages = emu_.Get<PageTableBuilder>();
        std::vector<std::pair<uint32_t, uint32_t>> image;
        for (const auto& xip : emu_.Get<RomParserService>().Primary().xips) {
            const auto& hdr = xip.toc.romhdr;
            if (hdr.physlast <= hdr.physfirst) continue;
            image.emplace_back(pages.VaToPa(hdr.physfirst), pages.VaToPa(hdr.physfirst) +
                                                            (hdr.physlast - hdr.physfirst));
        }
        std::sort(image.begin(), image.end());

        uint32_t       cursor = card_pa_ + kWordBytes;
        const uint32_t end    = card_pa_ + card_bytes_;
        for (const auto& span : image) {
            if (span.first > cursor) FillBlank(cursor, std::min(span.first, end));
            cursor = std::max(cursor, span.second);
        }
        if (cursor < end) FillBlank(cursor, end);
    }

    void FillBlank(uint32_t from, uint32_t to) {
        if (to <= from) return;
        const std::vector<uint8_t> blank(to - from, kBlankByte);
        emu_.Get<EmulatedMemory>().CopyIn(from, blank.data(), blank.size());
    }

    uint32_t Checksum() {
        if (!checksum_) {
            std::vector<uint8_t> card(card_bytes_ - kWordBytes);
            emu_.Get<EmulatedMemory>().CopyOut(card_pa_ + kWordBytes, card.data(), card.size());
            uint32_t sum = 0;
            for (uint32_t off = 0; off < card.size(); off += kWordBytes) {
                const uint32_t pa = card_pa_ + kWordBytes + off;
                if (pa >= kBootBlockPa && pa < kBootBlockEnd) continue;
                const uint32_t word = cerf::le::U32(card.data(), off);
                if (word != kBlankWord) sum += word;
            }
            checksum_ = sum;
        }
        return *checksum_;
    }

    uint32_t                card_pa_    = 0;
    uint32_t                card_bytes_ = 0;
    std::optional<uint32_t> checksum_;
};

}

REGISTER_SERVICE(IbmWorkpadZ50RomCardChecksum);

}
