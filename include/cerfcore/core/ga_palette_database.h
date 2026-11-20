#pragma once

#include "service.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace cerfcore {

struct DbGaPalette {
    std::string           id;
    std::string           name;
    uint32_t              bpp;
    std::vector<uint32_t> colors;
};

class GaPaletteDatabase : public Service {
public:
    using Service::Service;

    void OnReady() override;

    std::span<const DbGaPalette> Palettes() const { return palettes_; }
    const DbGaPalette*           DefaultFor(uint32_t bpp) const;

private:
    std::vector<DbGaPalette> palettes_;
};

}
