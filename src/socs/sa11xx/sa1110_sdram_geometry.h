#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

struct DbPart;

class Sa1110SdramGeometry : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t RowBits() const { return row_bits_; }
    uint32_t ColumnBits() const { return column_bits_; }
    uint32_t BankBits() const { return bank_bits_; }

private:
    const DbPart* RamPart() const;

    uint32_t row_bits_    = 0;
    uint32_t column_bits_ = 0;
    uint32_t bank_bits_   = 0;
};

}
