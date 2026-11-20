#include "vr4101_siu_dma.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "vr4101_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kSpaceWords = 1024u;

}

bool Vr4101SiuDma::ShouldRegister() { return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4101; }

Vr4101SiuDma::TxRun Vr4101SiuDma::Transmit(uint16_t dlength) {
    auto& dmaau = emu_.Get<Vr4101DmaAddresses>();
    TxRun run;
    for (uint32_t i = 0; i < kSpaceWords && !run.boundary; ++i) {
        run.last_word = dmaau.Load(Vr4101DmaChannel::kSiuTx);
        vr4101_siu::TxChar c;
        if (vr4101_siu::TransmittedChar(dlength, run.last_word, c)) run.chars.push_back(c);
        run.boundary = dmaau.Step(Vr4101DmaChannel::kSiuTx);
    }
    return run;
}

bool Vr4101SiuDma::Receive(uint16_t word) {
    return emu_.Get<Vr4101DmaAddresses>().Store(Vr4101DmaChannel::kSiuRx, word);
}

REGISTER_SERVICE(Vr4101SiuDma);

}
