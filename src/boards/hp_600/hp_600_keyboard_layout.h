#pragma once

#include <cerfcore/core/service.h>
#include "../jornada/jornada_keyboard_layers.h"
#include "../sh77xx_port_key_matrix.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace cerfcore {

inline constexpr uint32_t kHp600KeyCells = Sh77xxPortKeyMatrix::kCells;

struct Hp600HostAlias {
    uint8_t     host_vk;
    uint8_t     guest_vk;
    const char* label;
};

class Hp600KeyboardLayout : public Service {
public:
    using Service::Service;

    virtual const std::array<uint8_t, kHp600KeyCells>& CellVk() const = 0;
    virtual const std::array<uint16_t, Sh77xxPortKeyMatrix::kRows>& ScannedColumns() const = 0;
    virtual std::span<const Hp600HostAlias> HostAliases() const = 0;
    virtual std::span<const jornada_kbd::LayerKey> AltGrLayer() const = 0;

    bool                    Scanned(uint32_t cell) const;
    std::optional<uint32_t> ScannedCellOf(uint8_t vk) const;
};

}
