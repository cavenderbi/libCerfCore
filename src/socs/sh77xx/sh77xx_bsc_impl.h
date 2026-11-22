#pragma once

#include <cstdint>
#include <string_view>

#include "../reg_window_impl.h"
#include "sh77xx_area0_bus_width.h"
#include "sh77xx_bsc.h"
#include "sh77xx_refresh_timer.h"

namespace cerfcore {

namespace sh77xx_bsc {

using cerf_reg_window_detail::OtherReset;
using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegSpec;
using cerf_reg_window_detail::RegWindowBase;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;

inline constexpr uint32_t kBase           = 0x1FFFFF60u;
inline constexpr uint32_t kRegs           = 16u;
inline constexpr uint32_t kRegsBeforeBcr3 = 11u;

inline constexpr uint32_t kOffBcr1  = 0x00u;
inline constexpr uint32_t kOffBcr2  = 0x02u;
inline constexpr uint32_t kOffWcr1  = 0x04u;
inline constexpr uint32_t kOffWcr2  = 0x06u;
inline constexpr uint32_t kOffMcr   = 0x08u;
inline constexpr uint32_t kOffDcr   = 0x0Au;
inline constexpr uint32_t kOffPcr   = 0x0Cu;
inline constexpr uint32_t kOffRtcsr = 0x0Eu;
inline constexpr uint32_t kOffRtcnt = 0x10u;
inline constexpr uint32_t kOffRtcor = 0x12u;
inline constexpr uint32_t kOffRfcr  = 0x14u;
inline constexpr uint32_t kOffBcr3  = 0x1Eu;

inline constexpr uint16_t kBcr1EndianLittle  = 0x0800u;
inline constexpr uint32_t kBcr1A0BurstShift  = 9u;
inline constexpr uint16_t kBcr1A5Pcmcia      = 0x0002u;
inline constexpr uint16_t kBcr1A6Pcmcia      = 0x0001u;
inline constexpr uint32_t kBcr1DramTypeShift = 2u;

inline constexpr uint32_t kDramTypeSdramArea3     = 2u;
inline constexpr uint32_t kDramTypeSdramArea2And3 = 3u;

inline constexpr uint16_t kBcr2Reset     = 0x3FF0u;
inline constexpr uint16_t kWcr1Reset     = 0x3FF3u;
inline constexpr uint16_t kWcr2Reset     = 0xFFFFu;
inline constexpr uint16_t kDcrWritable   = 0xF35Eu;
inline constexpr uint16_t kBcr3Writable  = 0xBF3Fu;
inline constexpr uint16_t kBcr3WriteZero = 0x40C0u;

inline constexpr uint16_t kTpcHighBit      = 0x8000u;
inline constexpr uint16_t kBcr3Tpc3HighBit = 0x2000u;
inline constexpr uint16_t kBcr3Tpc2HighBit = 0x0020u;
inline constexpr uint32_t kMcrTrwlShift    = 10u;

inline constexpr uint32_t kBcr1A5BurstShift = 7u;
inline constexpr uint32_t kBcr1A6BurstShift = 5u;
inline constexpr uint32_t kBurstOf8         = 2u;
inline constexpr uint32_t kBurstOf16        = 3u;
inline constexpr uint32_t kMcrAmxShift      = 3u;

inline constexpr uint8_t kProgrammedBcr1 = 0x01u;
inline constexpr uint8_t kProgrammedBcr2 = 0x02u;
inline constexpr uint8_t kProgrammedMcr  = 0x04u;

inline constexpr uint16_t kRtcsrStored           = 0x007Bu;
inline constexpr uint16_t kRtcsrInterruptEnables = 0x0042u;
inline constexpr uint16_t kRtcorStored           = 0x00FFu;
inline constexpr uint16_t kRefreshKey            = 0xA5u;
inline constexpr uint16_t kRefreshData           = 0x00FFu;
inline constexpr uint16_t kRfcrKey               = 0x29u;
inline constexpr uint32_t kRfcrKeyShift          = 10u;
inline constexpr uint16_t kRfcrData              = 0x03FFu;

inline constexpr uint32_t kBusSizeWord           = 2u;
inline constexpr uint32_t kBusSizeLongword       = 3u;
inline constexpr uint32_t kWordBusBytes          = 2u;
inline constexpr uint32_t kLongwordBusBytes      = 4u;
inline constexpr uint32_t kDramTypeDramArea3     = 4u;
inline constexpr uint32_t kDramTypeDramArea2And3 = 5u;

struct Model {
    uint16_t bcr1_writable;
    uint16_t wcr1_writable;
    uint16_t mcr_writable;
    uint16_t pcr_writable;
    uint16_t pcr_write_zero;
    bool     tpc_high_codes_reserved;
    bool     tpc2_high_codes_reserved;
    bool     sdram;
    uint16_t bcr2_writable               = kBcr2Reset;
    uint16_t bcr2_reset                  = kBcr2Reset;
    uint16_t bcr2_port_enable            = 0u;
    uint32_t bcr2_first_area             = kSh77xxBcr2FirstArea;
    bool     area6_pcmcia_size_unchecked = true;
    uint16_t wcr1_reset                  = kWcr1Reset;
    uint16_t wcr1_write_zero             = 0u;
    uint16_t bcr1_reset                  = kBcr1EndianLittle;
    uint16_t bcr1_write_zero             = 0u;
    uint16_t mcr_write_zero              = 0u;
    uint16_t dcr_write_zero              = 0u;
    uint16_t mcr_dram_longword           = 0u;
    bool     bcr3                        = true;
    uint8_t  dramtp_area2_word           = 0u;
    uint8_t  dramtp_area3_word           = 0u;
    uint8_t  dramtp_same_size            = 0u;
    bool     area2_dram_limits_all_areas = false;
    uint16_t mcr_dram_size_as_area3      = 0u;
    uint16_t mcr_edo_mode                = 0u;
    uint16_t mcr_bank_active             = 0u;
    uint16_t mcr_amx                     = 0u;
    uint8_t  mcr_amx_dram_codes          = 0xFFu;
    uint8_t  mcr_amx_sdram_codes         = 0xFFu;
    bool     sdram_mode_register         = false;
    bool     mcr_longword_read           = false;
};

constexpr RegSpec Stored(uint16_t writable, uint16_t reset, uint16_t write_zero) {
    return RegSpec{ReadKind::kStored, WriteKind::kStored, writable, reset, write_zero, OtherReset::kRetain};
}

constexpr RegSpec StoredWithFatalRead(uint16_t writable, uint16_t write_zero) {
    return RegSpec{ReadKind::kFatal, WriteKind::kStored, writable, 0u, write_zero, OtherReset::kRetain};
}

constexpr RegSpec StoredWithSampledRead(uint16_t writable) {
    return RegSpec{ReadKind::kSampled, WriteKind::kStored, writable, 0u, 0u, OtherReset::kRetain};
}

constexpr RegWindowModel WindowOf(const Model& m) {
    const uint32_t regs = m.bcr3 ? kRegs : kRegsBeforeBcr3;
    RegWindowModel w{kBase, regs * 2u, regs, false, {}};
    w.reg[kOffBcr1 / 2u]  = Stored(m.bcr1_writable, m.bcr1_reset, m.bcr1_write_zero);
    w.reg[kOffBcr2 / 2u]  = Stored(m.bcr2_writable, m.bcr2_reset, 0u);
    w.reg[kOffWcr1 / 2u]  = Stored(m.wcr1_writable, m.wcr1_reset, m.wcr1_write_zero);
    w.reg[kOffWcr2 / 2u]  = Stored(kWcr2Reset, kWcr2Reset, 0u);
    w.reg[kOffMcr / 2u]   = Stored(m.mcr_writable, 0u, m.mcr_write_zero);
    w.reg[kOffDcr / 2u]   = Stored(kDcrWritable, 0u, m.dcr_write_zero);
    w.reg[kOffPcr / 2u]   = Stored(m.pcr_writable, 0u, m.pcr_write_zero);
    w.reg[kOffRtcsr / 2u] = StoredWithFatalRead(kRtcsrStored, kRtcsrInterruptEnables);
    w.reg[kOffRtcnt / 2u] = StoredWithFatalRead(kRefreshData, 0u);
    w.reg[kOffRtcor / 2u] = Stored(kRtcorStored, 0u, 0u);
    w.reg[kOffRfcr / 2u]  = StoredWithSampledRead(kRfcrData);
    if (m.bcr3) w.reg[kOffBcr3 / 2u] = Stored(kBcr3Writable, 0u, kBcr3WriteZero);
    return w;
}

template <const std::string_view& Soc, const RegWindowModel& W, const Model& M>
class Sh77xxBscBase : public RegWindowBase<Soc, W, Sh77xxBsc> {
    using Base = RegWindowBase<Soc, W, Sh77xxBsc>;

public:
    using Base::Base;

    void OnReady() override {
        Base::OnReady();
        area0_bus_ = this->emu_.template TryGet<Sh77xxArea0BusWidth>();
        refresh_.Attach();
        this->emu_.template Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
            std::lock_guard lk(this->lock_);
            if (this->IsPowerOnReset(kind)) {
                programmed_ = 0u;
                refresh_.PowerOnReset();
            }
        });
    }

    void SaveState(StateWriter& w) override {
        Base::SaveState(w);
        w.Write("programmed", programmed_);
        refresh_.Save(w);
    }

    void RestoreState(StateReader& r) override {
        Base::RestoreState(r);
        r.Read("programmed", programmed_);
        refresh_.Restore(r);
    }

    void PostRestore() override { refresh_.PostRestore(); }

    bool HasSdramModeRegister() const override { return M.sdram || M.sdram_mode_register; }

    bool PortEnabled() const override { return PortenSet(this->StoredReg(kOffBcr2 / 2u)); }

    bool AreaIsPcmcia(uint32_t area) const override {
        return (this->StoredReg(kOffBcr1 / 2u) & (area == 5u ? kBcr1A5Pcmcia : kBcr1A6Pcmcia)) != 0u;
    }

    uint32_t AreaBusBytes(uint32_t area) const override {
        return BusBytes(area, this->StoredReg(kOffBcr1 / 2u), this->StoredReg(kOffBcr2 / 2u),
                        this->StoredReg(kOffMcr / 2u));
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t off = addr - W.base;
        if (off == kOffRtcsr || off == kOffRtcnt || off == kOffRtcor) {
            if ((value >> 8) != kRefreshKey) {
                this->HaltUnsupportedAccess("WriteHalf of a refresh register without the H'A5 key", addr, value);
            }
            value = static_cast<uint16_t>(value & kRefreshData);
        } else if (off == kOffRfcr) {
            if ((value >> kRfcrKeyShift) != kRfcrKey) {
                this->HaltUnsupportedAccess("WriteHalf of RFCR without the B'101001 key", addr, value);
            }
            value = static_cast<uint16_t>(value & kRfcrData);
        }
        if (const char* why = WriteRejected(off, static_cast<uint16_t>(value & W.reg[off / 2u].wmask))) {
            this->HaltUnsupportedAccess(why, addr, value);
        }
        Base::WriteHalf(addr, value);
    }

    uint32_t ReadWord(uint32_t addr) override {
        if (!M.mcr_longword_read || addr - W.base != kOffMcr) this->HaltUnsupportedAccess("ReadWord", addr, 0);
        return this->ReadHalf(addr);
    }

protected:
    uint16_t SampleRegister(uint32_t i, uint32_t addr) override {
        if (i == kOffRfcr / 2u) return refresh_.RefreshCount();
        return Base::SampleRegister(i, addr);
    }

    void AfterStore(uint32_t i) override {
        if (i == kOffRtcsr / 2u) refresh_.WriteControl(this->StoredReg(i));
        if (i == kOffRtcnt / 2u) refresh_.WriteCount(this->StoredReg(i));
        if (i == kOffRtcor / 2u) refresh_.WriteConstant(this->StoredReg(i));
        if (i == kOffRfcr / 2u) refresh_.WriteRefreshCount(this->StoredReg(i));
        programmed_        = static_cast<uint8_t>(programmed_ | ProgrammedBit(i * 2u));
        bool width_changed = i == kOffBcr2 / 2u;
        if constexpr (M.mcr_dram_longword != 0u) {
            width_changed = width_changed || i == kOffBcr1 / 2u || i == kOffMcr / 2u;
        }
        if (width_changed) this->NotifyBusWidthChanged();
    }

private:
    static uint32_t DramType(uint16_t bcr1) { return (bcr1 >> kBcr1DramTypeShift) & 7u; }

    static uint32_t SizeCode(uint16_t bcr2, uint32_t area) { return (bcr2 >> (2u * area)) & 3u; }

    static bool PortenSet(uint16_t bcr2) { return (bcr2 & M.bcr2_port_enable) != 0u; }

    static uint8_t ProgrammedBit(uint32_t off) {
        if (off == kOffBcr1) return kProgrammedBcr1;
        if (off == kOffBcr2) return kProgrammedBcr2;
        if (off == kOffMcr) return kProgrammedMcr;
        return 0u;
    }

    static bool BurstTooLong(uint32_t burst, uint32_t bus_bytes) {
        return burst >= kBurstOf8 && bus_bytes > (burst == kBurstOf16 ? 1u : kWordBusBytes);
    }

    static uint32_t BusBytes(uint32_t area, [[maybe_unused]] uint16_t bcr1, uint16_t bcr2,
                             [[maybe_unused]] uint16_t mcr) {
        if constexpr (M.mcr_dram_longword != 0u) {
            const uint32_t dram_type = DramType(bcr1);
            if (area == 2u && dram_type == kDramTypeDramArea2And3) return kWordBusBytes;
            if (area == 3u && (dram_type == kDramTypeDramArea3 || dram_type == kDramTypeDramArea2And3)) {
                return (mcr & M.mcr_dram_longword) != 0u ? kLongwordBusBytes : kWordBusBytes;
            }
        }
        return (1u << SizeCode(bcr2, area)) >> 1;
    }

    const char* WriteRejected(uint32_t off, uint16_t v) const {
        if (off == kOffBcr1) {
            const uint32_t dram_type = DramType(v);
            if (dram_type == 1u || dram_type == 6u || dram_type == 7u) return "BCR1 DRAMTP selects a reserved memory type";
            if (!M.sdram && (dram_type == kDramTypeSdramArea3 || dram_type == kDramTypeSdramArea2And3)) {
                return "BCR1 DRAMTP selects a reserved memory type";
            }
            if (const char* why = A0BurstRejected((v >> kBcr1A0BurstShift) & 3u)) return why;
            return SettingsRejected(v, this->StoredReg(kOffBcr2 / 2u), this->StoredReg(kOffMcr / 2u),
                                    programmed_ | kProgrammedBcr1);
        } else if (off == kOffBcr2) {
            for (uint32_t area = M.bcr2_first_area; area <= kSh77xxBcr2LastArea; ++area) {
                if (SizeCode(v, area) != 0u) continue;
                if (M.area6_pcmcia_size_unchecked && area == kSh77xxBcr2LastArea && AreaIsPcmcia(area)) continue;
                return "BCR2 AnSZ selects the reserved bus size 00";
            }
            return SettingsRejected(this->StoredReg(kOffBcr1 / 2u), v, this->StoredReg(kOffMcr / 2u),
                                    programmed_ | kProgrammedBcr2);
        } else if (off == kOffMcr) {
            if (((v >> kMcrTrwlShift) & 3u) == 3u) return "MCR TRWL selects the reserved value 11";
            if (M.tpc_high_codes_reserved && (v & kTpcHighBit) != 0u) return "MCR TPC selects a reserved precharge time";
            return SettingsRejected(this->StoredReg(kOffBcr1 / 2u), this->StoredReg(kOffBcr2 / 2u), v,
                                    programmed_ | kProgrammedMcr);
        } else if (off == kOffDcr) {
            if (M.tpc_high_codes_reserved && (v & kTpcHighBit) != 0u) return "DCR TPC selects a reserved precharge time";
        } else if (off == kOffBcr3) {
            if (M.tpc_high_codes_reserved && (v & kBcr3Tpc3HighBit) != 0u) {
                return "BCR3 TPC3 selects a reserved precharge time";
            }
            if (M.tpc2_high_codes_reserved && (v & kBcr3Tpc2HighBit) != 0u) {
                return "BCR3 TPC2 selects a reserved precharge time";
            }
        }
        return nullptr;
    }

    const char* SettingsRejected(uint16_t bcr1, uint16_t bcr2, [[maybe_unused]] uint16_t mcr,
                                 uint32_t programmed) const {
        const bool     types     = (programmed & kProgrammedBcr1) != 0u;
        const bool     sizes     = (programmed & kProgrammedBcr2) != 0u;
        const uint32_t dram_type = DramType(bcr1);
        [[maybe_unused]] const bool memory     = (programmed & kProgrammedMcr) != 0u;
        [[maybe_unused]] const bool area3_dram = dram_type == kDramTypeDramArea3 || dram_type == kDramTypeDramArea2And3;
        [[maybe_unused]] const bool any_sdram  = dram_type == kDramTypeSdramArea3 || dram_type == kDramTypeSdramArea2And3;
        [[maybe_unused]] const uint32_t type_bit = 1u << dram_type;
        bool limited = sizes && PortenSet(bcr2);
        if constexpr (M.area2_dram_limits_all_areas) {
            limited = limited || (types && sizes && dram_type == kDramTypeDramArea2And3);
        }
        if (limited) {
            if (area0_bus_ == nullptr) return "PORTEN or DRAM in area 2 limits the unmodeled area 0 bus width";
            if (area0_bus_->Bytes() > kWordBusBytes) return "PORTEN or DRAM in area 2 with a longword bus in area 0";
            for (uint32_t area = M.bcr2_first_area; area <= kSh77xxBcr2LastArea; ++area) {
                if (SizeCode(bcr2, area) == kBusSizeLongword) {
                    return "PORTEN or DRAM in area 2 with a longword bus size in BCR2";
                }
            }
            if constexpr (M.mcr_dram_longword != 0u) {
                if (types && memory && area3_dram && (mcr & M.mcr_dram_longword) != 0u) {
                    return "PORTEN or DRAM in area 2 with a longword DRAM bus in MCR SZ";
                }
            }
        }
        if (types && sizes) {
            if constexpr (M.dramtp_area2_word != 0u) {
                if ((M.dramtp_area2_word & type_bit) != 0u && SizeCode(bcr2, 2u) != kBusSizeWord) {
                    return "BCR1 DRAMTP selects DRAM in area 2 without the word bus size in A2SZ";
                }
            }
            if constexpr (M.dramtp_area3_word != 0u) {
                if ((M.dramtp_area3_word & type_bit) != 0u && SizeCode(bcr2, 3u) != kBusSizeWord) {
                    return "BCR1 DRAMTP selects DRAM in areas 2 and 3 without the word bus size in A3SZ";
                }
            }
            if constexpr (M.dramtp_same_size != 0u) {
                if ((M.dramtp_same_size & type_bit) != 0u && SizeCode(bcr2, 2u) != SizeCode(bcr2, 3u)) {
                    return "BCR1 DRAMTP selects SDRAM in areas 2 and 3 with different bus sizes in A2SZ and A3SZ";
                }
            }
            if (BurstTooLong((bcr1 >> kBcr1A5BurstShift) & 3u, BusBytes(5u, bcr1, bcr2, mcr))) {
                return "BCR1 A5BST selects a burst length that the area 5 bus size does not allow";
            }
            if (BurstTooLong((bcr1 >> kBcr1A6BurstShift) & 3u, BusBytes(6u, bcr1, bcr2, mcr))) {
                return "BCR1 A6BST selects a burst length that the area 6 bus size does not allow";
            }
        }
        if constexpr (M.mcr_dram_size_as_area3 != 0u) {
            if (types && sizes && memory && area3_dram) {
                const uint32_t size = (mcr & M.mcr_dram_size_as_area3) != 0u ? kBusSizeLongword : kBusSizeWord;
                if (SizeCode(bcr2, 3u) != size) return "MCR SZ and BCR2 A3SZ select different bus sizes for the DRAM";
            }
        }
        if constexpr (M.mcr_edo_mode != 0u) {
            if (types && memory && any_sdram && (mcr & M.mcr_edo_mode) != 0u) {
                return "MCR EDOMODE is set while BCR1 DRAMTP selects SDRAM";
            }
        }
        if constexpr (M.mcr_bank_active != 0u) {
            if (types && memory && dram_type == kDramTypeSdramArea2And3 && (mcr & M.mcr_bank_active) != 0u) {
                return "MCR RASD selects the bank active mode while areas 2 and 3 are SDRAM";
            }
        }
        if constexpr (M.mcr_amx != 0u) {
            if (types && memory) {
                const uint32_t amx = (mcr & M.mcr_amx) >> kMcrAmxShift;
                if (area3_dram && ((M.mcr_amx_dram_codes >> amx) & 1u) == 0u) {
                    return "MCR AMX selects a code that is reserved for the DRAM interface";
                }
                if (any_sdram && ((M.mcr_amx_sdram_codes >> amx) & 1u) == 0u) {
                    return "MCR AMX selects a code that the SDRAM interface does not have";
                }
            }
        }
        return nullptr;
    }

    const char* A0BurstRejected(uint32_t a0bst) const {
        if (a0bst < kBurstOf8) return nullptr;
        if (area0_bus_ == nullptr) return "BCR1 A0BST selects a burst length that the unmodeled area 0 bus width limits";
        if (BurstTooLong(a0bst, area0_bus_->Bytes())) {
            return "BCR1 A0BST selects a burst length that the area 0 bus width does not allow";
        }
        return nullptr;
    }

    const Sh77xxArea0BusWidth* area0_bus_  = nullptr;
    uint8_t                    programmed_ = 0u;
    Sh77xxRefreshTimer         refresh_{this->emu_};
};

}

}
