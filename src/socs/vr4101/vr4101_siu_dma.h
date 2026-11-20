#pragma once

#include <cerfcore/core/service.h>
#include "vr4101_dma_addresses.h"
#include "vr4101_siu_frame.h"

#include <cstdint>
#include <vector>

namespace cerfcore {

class Vr4101SiuDma : public Service {
public:
    struct TxRun {
        std::vector<vr4101_siu::TxChar> chars;
        uint16_t                        last_word = 0u;
        bool                            boundary  = false;
    };

    using Service::Service;

    bool ShouldRegister() override;

    TxRun Transmit(uint16_t dlength);
    bool  Receive(uint16_t word);
};

}
