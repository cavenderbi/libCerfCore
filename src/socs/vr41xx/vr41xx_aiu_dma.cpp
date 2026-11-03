#include "vr41xx_aiu_dma.h"

namespace cerfcore {

void Vr41xxAiuDma::WriteDmaRegister(const std::function<void()>& write) {
    SyncDma();
    write();
    DmaRegistersWritten();
}

}
