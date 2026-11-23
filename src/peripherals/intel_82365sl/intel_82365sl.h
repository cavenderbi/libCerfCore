#pragma once

#include "../pcic82365/isa_exca_socket_pair.h"
#include "intel_82365sl_board.h"
#include "intel_82365sl_part_id.h"

#include <cstdint>
#include <string_view>

namespace cerfcore {

class Intel82365sl : public IsaExcaSocketPair {
public:
    explicit Intel82365sl(CerfEmulator& emu);

    static constexpr const char* kName                 = "82365SL";
    static constexpr bool        kPortOverlayUndefined = true;

    static constexpr std::string_view kPartId = PartId::Intel82365sl;

    bool ShouldRegister() override;
    void OnReady() override;

    void DriveResetDrive(bool high);

protected:
    const char* ChipName() const override { return kName; }
    uint8_t     UnbankedIndexReadLocked(uint32_t port) override;
    uint8_t     UnbankedDataReadLocked(uint32_t port) override;
    void        UnbankedDataWriteLocked(uint8_t) override {}
    bool        ChipOwns(uint8_t sub) const override;
    bool        ChipReadLocked(int bank, uint8_t sub, uint8_t* value) override;
    void        ChipWriteLocked(int bank, uint8_t sub, uint8_t value, ExcaWriteEffect* effect) override;
    uint8_t     InterfaceStatusLocked(int, uint8_t exca_status) override { return exca_status; }
    void        ResetChipLocked() override { index_valid_ = false; }
    void        CardDetectLocked(int) override {}
    uint32_t    CardAddressLocked(int, uint32_t card_addr) const override { return card_addr; }
    bool        InterfaceDisabledLocked(int) const override { return false; }
    bool        InpackGatedLocked(int) const override { return false; }
    uint32_t    MaskIoReadLocked(int, uint32_t, uint32_t value, unsigned) override { return value; }
    bool        Memcs16AssertedLocked(uint32_t off) override;
    void        UpdateIrqLineLocked() override;
    bool        SelectedLocked() const override;
    void        IndexWrittenLocked(uint8_t index) override;
    bool        ResetLineResets(ResetLineKind kind) override;
    void        SaveChipGlobal(StateWriter& w) const override;
    void        RestoreChipGlobal(StateReader& r) override;
    void        SaveChipBank(StateWriter&, int) const override {}
    void        RestoreChipBank(StateReader&, int) override {}
    void        AfterRestoreLocked() override {}

private:
    Intel82365slBoard* board_       = nullptr;
    bool               index_valid_ = false;
    bool               reset_drive_ = false;
};

}
