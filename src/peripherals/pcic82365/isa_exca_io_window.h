#pragma once

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../../core/board_parts.h"
#include "../peripheral_dispatcher.h"
#include "isa_host_bus_rule.h"

#include <cstdint>

namespace cerfcore {

template <class Controller, uint32_t kSize, class Host = UnrestrictedIsaHost>
class IsaExcaIoWindow : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardParts>().Has(Controller::kPartId);
    }

    void OnReady() override {
        pcic_ = &emu_.Get<Controller>();
        base_ = AreaBase();
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return base_; }
    uint32_t MmioSize() const override { return kSize; }

    void SaveState(StateWriter& w) override {
        if (CarriesControllerState()) pcic_->SaveState(w);
    }
    void RestoreState(StateReader& r) override {
        if (CarriesControllerState()) pcic_->RestoreState(r);
    }
    void PostRestore() override {
        if (CarriesControllerState()) pcic_->PostRestore();
    }

    uint8_t ReadByte(uint32_t addr) override {
        const uint32_t off  = addr - base_;
        const uint32_t port = PortOf(off);
        if (ControllerDecodes(off)) {
            if (ControllerPort(port)) {
                RequirePortCycle(1u, addr, off);
                return pcic_->ReadPcicByte(port - kPortIndex);
            }
            if (ChipHalfwordPort(port)) HaltUnsupportedAccess("ISA ExCA chip port byte read", addr, 0);
        }
        RequireCardIoSpace(off, addr, "ISA ExCA I/O byte read", 0u);
        if constexpr (Host::kRestricted) return pcic_->ReadCardIo8(host_, port);
        else return pcic_->ReadCardIo8(port);
    }

    uint16_t ReadHalf(uint32_t addr) override {
        const uint32_t off  = addr - base_;
        const uint32_t port = PortOf(off);
        if (ControllerDecodes(off)) {
            if (ChipHalfwordPort(port)) return ReadChipHalfwordPort(port);
            if (port == kPortIndex) {
                if (!IndexDataHalfword()) HaltUnsupportedAccess("ISA ExCA 16-bit index/data read", addr, 0);
                RequirePortCycle(2u, addr, off);
                const uint16_t index = pcic_->ReadPcicByte(0u);
                const uint16_t data  = pcic_->ReadPcicByte(1u);
                return static_cast<uint16_t>(index | (data << 8));
            }
        }
        RequireCardIoSpace(off, addr, "ISA ExCA I/O halfword read", 0u);
        if constexpr (Host::kRestricted) return pcic_->ReadCardIo16(host_, port);
        else return pcic_->ReadCardIo16(port);
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        const uint32_t off  = addr - base_;
        const uint32_t port = PortOf(off);
        if (ControllerDecodes(off)) {
            if (ControllerPort(port)) {
                RequirePortCycle(1u, addr, off);
                pcic_->WritePcicByte(port - kPortIndex, value);
                return;
            }
            if (ChipHalfwordPort(port)) HaltUnsupportedAccess("ISA ExCA chip port byte write", addr, value);
        }
        RequireCardIoSpace(off, addr, "ISA ExCA I/O byte write", value);
        if constexpr (Host::kRestricted) pcic_->WriteCardIo8(host_, port, value);
        else pcic_->WriteCardIo8(port, value);
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t off  = addr - base_;
        const uint32_t port = PortOf(off);
        if (ControllerDecodes(off)) {
            if (ChipHalfwordPort(port)) {
                WriteChipHalfwordPort(port, value);
                return;
            }
            if (port == kPortIndex) {
                if (!IndexDataHalfword()) HaltUnsupportedAccess("ISA ExCA 16-bit index/data write", addr, value);
                RequirePortCycle(2u, addr, off);
                pcic_->WritePcicByte(0u, static_cast<uint8_t>(value & 0xFFu));
                pcic_->WritePcicByte(1u, static_cast<uint8_t>(value >> 8));
                return;
            }
        }
        RequireCardIoSpace(off, addr, "ISA ExCA I/O halfword write", value);
        if constexpr (Host::kRestricted) pcic_->WriteCardIo16(host_, port, value);
        else pcic_->WriteCardIo16(port, value);
    }

    uint32_t ReadWord  (uint32_t addr) override { return ReadWordAsHalves(addr); }
    void     WriteWord (uint32_t addr, uint32_t value) override { WriteWordAsHalves(addr, value); }
    uint64_t ReadDword (uint32_t addr) override { return ReadDwordAsHalves(addr); }
    void     WriteDword(uint32_t addr, uint64_t value) override { WriteDwordAsHalves(addr, value); }

protected:
    static constexpr uint32_t kPortIndex = 0x3E0u;
    static constexpr uint32_t kPortData  = 0x3E1u;

    virtual uint32_t AreaBase() = 0;
    virtual bool     CarriesControllerState() const { return true; }
    virtual uint32_t PortOf(uint32_t off) const { return off; }
    virtual bool     ControllerDecodes(uint32_t) { return true; }
    virtual bool     CardIoSpace(uint32_t) const { return true; }
    virtual bool     IndexDataHalfword() const { return true; }
    virtual bool     ChipHalfwordPort(uint32_t) const { return false; }
    virtual uint16_t ReadChipHalfwordPort(uint32_t port) { HaltUnsupportedAccess("ISA ExCA chip port read", port, 0); }
    virtual void     WriteChipHalfwordPort(uint32_t port, uint16_t value) {
        HaltUnsupportedAccess("ISA ExCA chip port write", port, value);
    }

    Controller& Pcic() { return *pcic_; }

private:
    static bool ControllerPort(uint32_t port) { return port == kPortIndex || port == kPortData; }

    void RequireCardIoSpace(uint32_t off, uint32_t addr, const char* op, uint64_t value) {
        if (!CardIoSpace(off)) HaltUnsupportedAccess(op, addr, value);
    }

    void RequirePortCycle(unsigned bytes, uint32_t addr, uint32_t off) {
        if constexpr (Host::kRestricted) {
            if (!host_.AllowsCycle(false, bytes, addr))
                emu_.Get<Fatal>().Die("%s: host refuses a %u-byte cycle at 0x%08X to the 8-bit controller ports",
                                      Controller::kName, bytes, addr);
        }
        if constexpr (Controller::kPortOverlayUndefined) {
            if (pcic_->CardIoDecodes(off))
                emu_.Get<Fatal>().Die("%s: an enabled card I/O window covers the controller port at 0x%08X",
                                      Controller::kName, addr);
        }
    }

    Host        host_{};
    Controller* pcic_ = nullptr;
    uint32_t    base_ = 0u;
};

}
