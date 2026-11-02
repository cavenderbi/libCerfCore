#include "pci1225_exca_chip_registers.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint8_t kRegIdRevision     = 0x00u;
constexpr uint8_t kRegCardDetectCtrl = 0x16u;
constexpr uint8_t kRegGlobalCtrl     = 0x1Eu;

constexpr uint8_t kIdWritable = 0x3Fu;

constexpr uint8_t kCdVs2High      = 0x80u;
constexpr uint8_t kCdVs1High      = 0x40u;
constexpr uint8_t kCdSoftwareCsc  = 0x20u;
constexpr uint8_t kCdResume       = 0x10u;
constexpr uint8_t kCdRegConfig    = 0x02u;

constexpr uint8_t kGlobalWritable  = 0x1Fu;
constexpr uint8_t kGlobalPowerDown = 0x01u;

}

bool Pci1225ExcaChipRegisters::Owns(uint8_t idx) {
    return idx == kRegIdRevision || idx == kRegCardDetectCtrl || idx == kRegGlobalCtrl;
}

uint8_t Pci1225ExcaChipRegisters::Read(uint8_t idx, bool vs1_grounded, bool vs2_grounded,
                                       uint8_t global_control) const {
    if (idx == kRegIdRevision)
        return id_revision_;
    if (idx == kRegCardDetectCtrl)
        return static_cast<uint8_t>((vs2_grounded ? 0u : kCdVs2High) | (vs1_grounded ? 0u : kCdVs1High) |
                                    cd_control_);
    return global_control;
}

Pci1225ExcaChipWrite Pci1225ExcaChipRegisters::Write(uint8_t idx, uint8_t value, bool id_writable,
                                                     uint8_t* global_control) {
    Pci1225ExcaChipWrite result;
    if (idx == kRegIdRevision) {
        if (id_writable)
            id_revision_ = static_cast<uint8_t>((id_revision_ & ~kIdWritable) | (value & kIdWritable));
        return result;
    }
    if (idx == kRegCardDetectCtrl) {
        if ((value & kCdRegConfig) != 0u) {
            result.refused = "sets REGCONFIG, whose register set SCPS035B Table 55 does not give";
            return result;
        }
        cd_control_ = static_cast<uint8_t>(value & kCdResume);
        result.software_csc = (value & kCdSoftwareCsc) != 0u;
        return result;
    }
    if ((value & kGlobalPowerDown) != 0u) {
        result.refused = "sets PWRDWN, which floats the card RESET output between card cycles";
        return result;
    }
    *global_control = static_cast<uint8_t>(value & kGlobalWritable);
    result.global_changed = true;
    return result;
}

void Pci1225ExcaChipRegisters::Reset() {
    id_revision_ = kIdRevisionReset;
    cd_control_  = 0u;
}

void Pci1225ExcaChipRegisters::SaveState(StateWriter& w) const {
    w.Write("cd_control", cd_control_);
    w.Write("exca_id_revision", id_revision_);
}

void Pci1225ExcaChipRegisters::RestoreState(StateReader& r) {
    r.Read("cd_control", cd_control_);
    r.Read("exca_id_revision", id_revision_);
}

}
