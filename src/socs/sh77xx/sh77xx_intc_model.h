#pragma once

#include "sh77xx_intc.h"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace cerfcore {

namespace sh77xx_intc {

enum class Scope : uint8_t { kAll, kStandbyWake };

struct Model {
    bool     irls;
    bool     lcdc_pcc;
    uint32_t standby_wake_irq_pins;
    bool     standby_wake_pint;
    bool     area1_registers = true;
    bool     area7_extra_ipr = false;
    bool     irq_pins        = true;

    uint32_t SelectableIrqPins(Scope scope, uint32_t irl_pins) const;
    bool     SelectsPint(Scope scope) const;
    uint16_t IprfHaltBits() const;
};

inline constexpr uint32_t kArea1Base = 0x04000000u;
inline constexpr uint32_t kArea1Size = 0x1Eu;

inline constexpr uint32_t kOffIntevt2 = 0x00u;
inline constexpr uint32_t kOffIrr0    = 0x04u;
inline constexpr uint32_t kOffIrr1    = 0x06u;
inline constexpr uint32_t kOffIrr2    = 0x08u;
inline constexpr uint32_t kOffIrr3    = 0x0Au;
inline constexpr uint32_t kOffIrr4    = 0x0Cu;
inline constexpr uint32_t kOffIcr1    = 0x10u;
inline constexpr uint32_t kOffIcr2    = 0x12u;
inline constexpr uint32_t kOffPinter  = 0x14u;
inline constexpr uint32_t kOffIprc    = 0x16u;
inline constexpr uint32_t kOffIprd    = 0x18u;
inline constexpr uint32_t kOffIpre    = 0x1Au;
inline constexpr uint32_t kOffIprf    = 0x1Cu;
inline constexpr uint32_t kOffIcr0    = 0x0u;
inline constexpr uint32_t kOffIpra    = 0x2u;
inline constexpr uint32_t kOffIprb    = 0x4u;
inline constexpr uint32_t kOffArea7ExtraIpr   = 0x6u;
inline constexpr uint32_t kArea7ExtraIprBytes = 2u;

enum Ipr : uint8_t { kIpra, kIprb, kIprc, kIprd, kIpre, kIprf, kIprArea7Extra, kIprCount };

inline constexpr uint16_t kIcr0Nmil     = 0x8000u;
inline constexpr uint16_t kIcr0Nmie     = 0x0100u;
inline constexpr uint16_t kIcr1Reset    = 0x4000u;
inline constexpr uint16_t kIcr1Mai      = 0x8000u;
inline constexpr uint16_t kIcr1IrqLevel = 0x4000u;
inline constexpr uint16_t kIcr1Blmsk    = 0x2000u;
inline constexpr uint16_t kIcr1Irlsen   = 0x1000u;
inline constexpr uint16_t kIprbReserved = 0x000Fu;
inline constexpr uint16_t kIprfReserved = 0xF000u;
inline constexpr uint16_t kArea7ExtraReserved  = 0x000Fu;
inline constexpr uint32_t kArea7ExtraHighShift = 12u;
inline constexpr uint32_t kArea7ExtraMidShift  = 8u;

inline constexpr uint32_t kIrqPins       = 6u;
inline constexpr uint32_t kIrlPins       = 4u;
inline constexpr uint32_t kPintBit0      = static_cast<uint32_t>(Sh77xxIntcPin::kPint0);
inline constexpr uint32_t kIrlsBit0      = static_cast<uint32_t>(Sh77xxIntcPin::kIrls0);
inline constexpr uint32_t kIrlsPins      = 0xFu << kIrlsBit0;
inline constexpr uint32_t kAllPinsHigh   = (1u << static_cast<uint32_t>(Sh77xxIntcPin::kCount)) - 1u;
inline constexpr uint32_t kSenseFalling  = 0u;
inline constexpr uint32_t kSenseRising   = 1u;
inline constexpr uint32_t kSenseLow      = 2u;
inline constexpr uint32_t kSenseReserved = 3u;
inline constexpr uint32_t kIrlNone       = 0xFu;
inline constexpr uint32_t kPintPinMask    = 0xFFFFu;
inline constexpr uint32_t kPintGroupMask  = 0x00FFu;
inline constexpr uint32_t kPintGroupShift = 8u;
inline constexpr uint32_t kPintGroupBits[2] = {kPintGroupMask, kPintGroupMask << kPintGroupShift};
inline constexpr uint8_t  kIrr0Pint0R    = 0x80u;
inline constexpr uint8_t  kIrr0Pint1R    = 0x40u;
inline constexpr uint32_t kIrr0IrqMask   = 0x3Fu;
inline constexpr uint32_t kIrr1Mask      = 0xFFu;
inline constexpr uint32_t kIrr2Shift     = static_cast<uint32_t>(Sh77xxIntcSource::kEri2);
inline constexpr uint32_t kIrr2Mask      = 0x1Fu;
inline constexpr uint32_t kIrr2LcdiMask  = 0x3Fu;
inline constexpr uint32_t kIrr3Shift     = static_cast<uint32_t>(Sh77xxIntcSource::kPc0Bdi);
inline constexpr uint32_t kIrr3Mask      = 0x7Fu;
inline constexpr uint32_t kIrr4Shift     = static_cast<uint32_t>(Sh77xxIntcSource::kPc1Bdi);
inline constexpr uint32_t kIrr4Mask      = 0x1Fu;
inline constexpr uint32_t kLevelCodeBase = 0x200u;
inline constexpr uint32_t kLevelCodeStep = 0x20u;
inline constexpr uint32_t kMaxLevel      = 15u;
inline constexpr uint32_t kNmiLevel      = 16u;
inline constexpr uint32_t kNmiIntevt     = 0x1C0u;

constexpr uint64_t SourceBit(Sh77xxIntcSource source) { return uint64_t{1} << static_cast<uint32_t>(source); }

inline constexpr uint64_t kStandbyWakeModules =
    SourceBit(Sh77xxIntcSource::kAti) | SourceBit(Sh77xxIntcSource::kPri) | SourceBit(Sh77xxIntcSource::kCui);

inline constexpr uint64_t kArea7ExtraSources =
    SourceBit(Sh77xxIntcSource::kIntevt6C0) | SourceBit(Sh77xxIntcSource::kIntevt6E0) |
    SourceBit(Sh77xxIntcSource::kIntevt700) | SourceBit(Sh77xxIntcSource::kIntevt720) |
    SourceBit(Sh77xxIntcSource::kIntevt740) | SourceBit(Sh77xxIntcSource::kIntevt760);

struct SourceRow {
    Ipr      ipr;
    uint8_t  shift;
    bool     level_coded;
    uint16_t intevt2;
};

inline constexpr SourceRow kIrqRows[kIrqPins] = {
    {kIprc, 0u, true, 0x600u}, {kIprc, 4u, true, 0x620u}, {kIprc, 8u, true, 0x640u},
    {kIprc, 12u, true, 0x660u}, {kIprd, 0u, true, 0x680u}, {kIprd, 4u, true, 0x6A0u},
};

inline constexpr SourceRow kPintRows[2] = {{kIprd, 12u, true, 0x700u}, {kIprd, 8u, true, 0x720u}};

inline constexpr SourceRow kModuleRows[] = {
    {kIpre, 12u, true, 0x800u},  {kIpre, 12u, true, 0x820u},  {kIpre, 12u, true, 0x840u},
    {kIpre, 12u, true, 0x860u},  {kIpre, 8u, true, 0x880u},   {kIpre, 8u, true, 0x8A0u},
    {kIpre, 8u, true, 0x8C0u},   {kIpre, 8u, true, 0x8E0u},   {kIpre, 4u, true, 0x900u},
    {kIpre, 4u, true, 0x920u},   {kIpre, 4u, true, 0x940u},   {kIpre, 4u, true, 0x960u},
    {kIpre, 0u, true, 0x980u},   {kIprf, 8u, true, 0x9A0u},   {kIprf, 4u, true, 0x9C0u},
    {kIprf, 4u, true, 0x9C0u},   {kIprf, 4u, true, 0x9C0u},   {kIprf, 4u, true, 0x9C0u},
    {kIprf, 4u, true, 0x9C0u},   {kIprf, 4u, true, 0x9C0u},   {kIprf, 4u, true, 0x9C0u},
    {kIprf, 0u, true, 0x9E0u},   {kIprf, 0u, true, 0x9E0u},   {kIprf, 0u, true, 0x9E0u},
    {kIprf, 0u, true, 0x9E0u},   {kIprf, 0u, true, 0x9E0u},   {kIpra, 12u, false, 0x400u},
    {kIpra, 8u, false, 0x420u},  {kIpra, 4u, false, 0x440u},  {kIpra, 4u, false, 0x460u},
    {kIpra, 0u, false, 0x480u},  {kIpra, 0u, false, 0x4A0u},  {kIpra, 0u, false, 0x4C0u},
    {kIprb, 4u, false, 0x4E0u},  {kIprb, 4u, false, 0x500u},  {kIprb, 4u, false, 0x520u},
    {kIprb, 4u, false, 0x540u},  {kIprb, 12u, false, 0x560u}, {kIprb, 8u, false, 0x580u},
    {kIprb, 8u, false, 0x5A0u},
    {kIprArea7Extra, kArea7ExtraHighShift, false, 0x6C0u},
    {kIprArea7Extra, kArea7ExtraMidShift, false, 0x6E0u},
    {kIprArea7Extra, 4u, false, 0x700u},
    {kIprArea7Extra, 4u, false, 0x720u},
    {kIprArea7Extra, 4u, false, 0x740u},
    {kIprArea7Extra, 4u, false, 0x760u},
};

static_assert(std::size(kModuleRows) == static_cast<size_t>(Sh77xxIntcSource::kCount),
              "every SH77xx on-chip interrupt source has a row");

struct Selection {
    uint32_t level   = 0u;
    uint32_t intevt  = 0u;
    uint32_t intevt2 = 0u;
};

constexpr uint32_t LevelCode(uint32_t level) { return kLevelCodeBase + (kMaxLevel - level) * kLevelCodeStep; }

}

}
