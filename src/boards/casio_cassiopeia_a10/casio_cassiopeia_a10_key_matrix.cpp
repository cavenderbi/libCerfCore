#include "casio_cassiopeia_a10_key_matrix.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/hibernation.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_on_key.h"
#include "casio_cassiopeia_a10_request_lines.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10KeyMatrix);

namespace {

constexpr uint32_t kWordE0     = 0x100000E0u;
constexpr uint32_t kWordE2     = 0x100000E2u;
constexpr uint32_t kRowSelect  = 0x100000E4u;
constexpr uint32_t kColumnWord = 0x100000E6u;

constexpr uint16_t kWordE0Value = 0x0001u;
constexpr uint16_t kWordE2Value = 0x0000u;
constexpr uint16_t kNoRows      = 0x0000u;
constexpr uint16_t kAllRows     = 0x01FFu;
constexpr uint16_t kAllLines    = 0xFFFFu;
constexpr uint16_t kRowBits     = 0x01FFu;

constexpr uint8_t kWrittenE0 = 0x01u;
constexpr uint8_t kWrittenE2 = 0x02u;
constexpr uint8_t kWrittenE4 = 0x04u;

bool RowSelectAccepted(uint16_t value) {
    if (value == kNoRows || value == kAllRows || value == kAllLines) return true;
    for (uint32_t row = 0; row < CasioCassiopeiaA10KeyMatrix::kRows; ++row) {
        if (value == static_cast<uint16_t>(1u << row)) return true;
    }
    return false;
}

}

bool CasioCassiopeiaA10KeyMatrix::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
}

void CasioCassiopeiaA10KeyMatrix::OnReady() {
    requests_ = &emu_.Get<CasioCassiopeiaA10RequestLines>();
    on_key_   = &emu_.Get<CasioCassiopeiaA10OnKey>();
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Reset(); });
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] {
        cells_.ReleaseAll();
        DriveMatrixLine();
        if (outside_key_) SetOutsideKey(false);
        on_key_->SetHeld(false);
    });
}

uint32_t CasioCassiopeiaA10KeyMatrix::MmioBase() const { return kWordE0; }
uint32_t CasioCassiopeiaA10KeyMatrix::MmioSize() const { return kColumnWord + 2u - kWordE0; }

uint16_t CasioCassiopeiaA10KeyMatrix::ReadHalf(uint32_t addr) {
    if (addr != kColumnWord || (written_ & kWrittenE4) == 0u) HaltUnsupportedAccess("ReadHalf", addr, 0u);
    return static_cast<uint16_t>(~cells_.Pressed(rows_ & kRowBits));
}

void CasioCassiopeiaA10KeyMatrix::WriteHalf(uint32_t addr, uint16_t value) {
    switch (addr) {
    case kWordE0:
        if (value != kWordE0Value) HaltUnsupportedAccess("WriteHalf", addr, value);
        word_e0_ = value;
        written_ = static_cast<uint8_t>(written_ | kWrittenE0);
        return;
    case kWordE2:
        if (value != kWordE2Value) HaltUnsupportedAccess("WriteHalf", addr, value);
        word_e2_ = value;
        written_ = static_cast<uint8_t>(written_ | kWrittenE2);
        return;
    case kRowSelect:
        if (!RowSelectAccepted(value)) HaltUnsupportedAccess("WriteHalf", addr, value);
        rows_    = value;
        written_ = static_cast<uint8_t>(written_ | kWrittenE4);
        DriveMatrixLine();
        return;
    case kColumnWord:
        if (value != 0u && value != kAllLines) HaltUnsupportedAccess("WriteHalf", addr, value);
        word_e6_ = value;
        return;
    default:
        HaltUnsupportedAccess("WriteHalf", addr, value);
    }
}

void CasioCassiopeiaA10KeyMatrix::SaveState(StateWriter& w) {
    w.Write("rows", rows_);
    w.Write("word_e0", word_e0_);
    w.Write("word_e2", word_e2_);
    w.Write("word_e6", word_e6_);
    w.Write("written", written_);
    w.Write("outside_key", outside_key_);
}

void CasioCassiopeiaA10KeyMatrix::RestoreState(StateReader& r) {
    r.Read("rows", rows_);
    r.Read("word_e0", word_e0_);
    r.Read("word_e2", word_e2_);
    r.Read("word_e6", word_e6_);
    r.Read("written", written_);
    r.Read("outside_key", outside_key_);
}

void CasioCassiopeiaA10KeyMatrix::SetKey(uint32_t code, bool down) {
    if (code == kOutsideKey) {
        if (down != outside_key_) SetOutsideKey(down);
        return;
    }
    if (code == kOnKey) {
        on_key_->SetHeld(down);
        return;
    }
    if (cells_.Set(code, down)) DriveMatrixLine();
}

void CasioCassiopeiaA10KeyMatrix::SetOutsideKey(bool down) {
    outside_key_ = down;
    requests_->Drive(CasioCassiopeiaA10Request::kOutsideKey, down);
}

void CasioCassiopeiaA10KeyMatrix::DriveMatrixLine() {
    requests_->Drive(CasioCassiopeiaA10Request::kKeyMatrix, cells_.Pressed(rows_ & kRowBits) != 0u);
}

void CasioCassiopeiaA10KeyMatrix::Reset() {
    rows_    = kNoRows;
    word_e0_ = 0u;
    word_e2_ = 0u;
    word_e6_ = 0u;
    written_ = 0u;
    DriveMatrixLine();
}

}
