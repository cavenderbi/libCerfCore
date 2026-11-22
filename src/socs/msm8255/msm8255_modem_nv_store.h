#pragma once

#include <cerfcore/core/service.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cerfcore {

inline constexpr uint32_t kModemNvItemDataBytes = 128u;

class Msm8255ModemNvStore : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    bool Find(uint32_t item, uint8_t (&data)[kModemNvItemDataBytes]) const;
    bool HoldsAnyIndex(uint32_t item) const;
    void Store(uint32_t item, const uint8_t (&data)[kModemNvItemDataBytes]);

private:
    void   Seed();
    void   Load(std::vector<uint8_t> records, const std::string& source);
    void   Persist();
    size_t RecordOffset(uint32_t item, uint32_t index) const;

    std::string          path_;
    std::vector<uint8_t> records_;
};

}
