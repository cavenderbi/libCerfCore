#pragma once

#include "../dram_bank_open_bus.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

class Hp600DramBankOpenBus : public DramBankOpenBus {
public:
    using DramBankOpenBus::DramBankOpenBus;

protected:
    virtual std::string_view ProbingSocId() const = 0;

    uint32_t         BankBytes() const override;
    std::string_view WindowBoardFamilyId() const override;
    bool             WindowOnBoard(const BoardContext& bd) const override;
};

}
