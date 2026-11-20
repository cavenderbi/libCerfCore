#include "vr41xx_piu_converter.h"

#include <optional>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/state/state_stream.h>
#include "vr41xx_cmu.h"
#include "vr41xx_buffered_piu.h"
#include "vr41xx_piu_panel.h"
#include "vr41xx_piu_regs.h"

namespace cerfcore {

using namespace cerf_vr41xx_piu_detail;

REGISTER_SERVICE(Vr41xxPiuConverter);

namespace {

constexpr uint16_t kLoadedFilmSample = 0u;

struct FilmDrive {
    bool     out0;
    bool     out1;
    uint16_t level0;
    uint16_t level1;
};

FilmDrive XFilm(uint16_t cmd) {
    return {(cmd & 0x0100u) != 0u, (cmd & 0x0200u) != 0u, (cmd & 0x0010u) ? kAdcMax : uint16_t{0},
            (cmd & 0x0020u) ? kAdcMax : uint16_t{0}};
}

FilmDrive YFilm(uint16_t cmd) {
    return {(cmd & 0x0400u) != 0u, (cmd & 0x0800u) != 0u, (cmd & 0x0040u) ? kAdcMax : uint16_t{0},
            (cmd & 0x0080u) ? kAdcMax : uint16_t{0}};
}

std::optional<uint16_t> FilmLevelAt(const FilmDrive& film, uint16_t pos) {
    if (film.out0 && film.out1) {
        const int32_t span = static_cast<int32_t>(film.level1) - static_cast<int32_t>(film.level0);
        return static_cast<uint16_t>(film.level0 + span * static_cast<int32_t>(pos & 0x3FFu) / kAdcMax);
    }
    if (film.out0) return film.level0;
    if (film.out1) return film.level1;
    return std::nullopt;
}

uint16_t FilmPortSample(uint16_t cmd, uint16_t adcmd, uint16_t pos_x, uint16_t pos_y, bool contact,
                        bool film) {
    const bool      x_film = adcmd < 2u;
    const bool      end1   = (adcmd & 1u) != 0u;
    const FilmDrive own    = x_film ? XFilm(cmd) : YFilm(cmd);
    const FilmDrive other  = x_film ? YFilm(cmd) : XFilm(cmd);
    if (end1 ? own.out1 : own.out0) return end1 ? own.level1 : own.level0;
    if (!film) return kUndrivenPortSample;
    const std::optional<uint16_t> other_level = FilmLevelAt(other, x_film ? pos_x : pos_y);
    if (const std::optional<uint16_t> own_level = FilmLevelAt(own, x_film ? pos_y : pos_x)) {
        return contact && other_level ? kLoadedFilmSample : *own_level;
    }
    return contact && other_level ? *other_level : kUndrivenPortSample;
}

/* Page-buffer slots, from PIUPB00REG at piu2_base (VR4121 UM Table 20-1 == VR4102 UM
   Table 19-1): PIUPBn0-3 at +0x00/02/04/06 (page 0) and +0x08/0A/0C/0E (page 1),
   PIUPB04REG at +0x1C and PIUPB14REG at +0x1E. */
bool PageBufferSlot(uint32_t off, int* page, int* idx) {
    switch (off) {
        case 0x00: *page = 0; *idx = 0; return true;
        case 0x02: *page = 0; *idx = 1; return true;
        case 0x04: *page = 0; *idx = 2; return true;
        case 0x06: *page = 0; *idx = 3; return true;
        case 0x1C: *page = 0; *idx = 4; return true;
        case 0x08: *page = 1; *idx = 0; return true;
        case 0x0A: *page = 1; *idx = 1; return true;
        case 0x0C: *page = 1; *idx = 2; return true;
        case 0x0E: *page = 1; *idx = 3; return true;
        case 0x1E: *page = 1; *idx = 4; return true;
        default:   return false;
    }
}

}

bool Vr41xxPiuConverter::ShouldRegister() { return emu_.TryGet<Vr41xxBufferedPiu>() != nullptr; }

void Vr41xxPiuConverter::OnReady() { emu_.Get<PeripheralDispatcher>().Register(this); }

uint32_t Vr41xxPiuConverter::MmioBase() const { return emu_.Get<Vr41xxBufferedPiu>().Piu2Base(); }
uint32_t Vr41xxPiuConverter::MmioSize() const { return emu_.Get<Vr41xxBufferedPiu>().Piu2Size(); }

int Vr41xxPiuConverter::ConvertCoordinates(uint16_t pos_x, uint16_t pos_y, bool contact) {
    emu_.Get<Vr41xxCmu>().RequireClock(kCmuMskPiu, "PIU coordinate sample");
    Vr41xxPiuPanel& panel = emu_.Get<Vr41xxPiuPanel>();
    std::lock_guard<std::mutex> lk(mtx_);
    const int page = next_page_;
    uint16_t (&buf)[5] = page_buf_[page];
    if (contact) {
        buf[0] = kValid | static_cast<uint16_t>(pos_x & 0x3FFu);
        buf[1] = kValid | static_cast<uint16_t>((kAdcMax - pos_x) & 0x3FFu);
        buf[2] = kValid | static_cast<uint16_t>(pos_y & 0x3FFu);
        buf[3] = kValid | static_cast<uint16_t>((kAdcMax - pos_y) & 0x3FFu);
    } else {
        const uint16_t plate = kValid | static_cast<uint16_t>(panel.ReleasedPlateSample() & 0x3FFu);
        for (int i = 0; i < 4; ++i) buf[i] = plate;
    }
    const std::optional<uint16_t> z = panel.PressureSample(contact);
    buf[4] = z ? static_cast<uint16_t>(kValid | (*z & 0x3FFu)) : 0u;
    next_page_ ^= 1;
    return page;
}

bool Vr41xxPiuConverter::ConvertCommand(uint16_t cmd, uint16_t pos_x, uint16_t pos_y, bool contact) {
    emu_.Get<Vr41xxCmu>().RequireClock(kCmuMskPiu, "PIU command scan");
    const uint16_t adcmd = static_cast<uint16_t>(cmd & 0x000Fu);
    if (adcmd == kAdcmdStandby) return false;
    if (adcmd > kAdcmdAudioIn) {
        emu_.Get<Fatal>().Die("VR41xx PIU: command scan with RFU ADCMD 0x%X (PIUCMDREG 0x%04X)", adcmd, cmd);
    }
    Vr41xxPiuPanel& panel = emu_.Get<Vr41xxPiuPanel>();
    const std::optional<uint16_t> val = adcmd < kAdcmdAdin0
        ? std::optional<uint16_t>(FilmPortSample(cmd, adcmd, pos_x, pos_y, contact, panel.HasTouchscreen()))
        : panel.PortSample(adcmd);
    std::lock_guard<std::mutex> lk(mtx_);
    adbuf_[0] = val ? static_cast<uint16_t>(kValid | (*val & 0x3FFu)) : 0u;
    return val.has_value();
}

/* Buffer slot n's port = (TPPSCAN?0:4)+n; PIUAMSKREG bit index = that port (VR4121 UM
   Table 20-5 + 20.3.7, VR4102 UM Table 19-5 + 19.3.7). */
bool Vr41xxPiuConverter::ScanAdPorts(uint16_t ascn, uint16_t amsk) {
    emu_.Get<Vr41xxCmu>().RequireClock(kCmuMskPiu, "PIU A/D port scan");
    std::lock_guard<std::mutex> lk(mtx_);
    const uint16_t port_base = (ascn & kTppScan) ? 0u : 4u;
    bool any_valid = false;
    for (uint16_t slot = 0; slot < 4u; ++slot) {
        const uint16_t port = port_base + slot;
        if (amsk & (1u << port)) { adbuf_[slot] = 0; continue; }
        const std::optional<uint16_t> v = port < kAdcmdAdin0
            ? std::optional<uint16_t>(kUndrivenPortSample)
            : emu_.Get<Vr41xxPiuPanel>().PortSample(port);
        if (v) { adbuf_[slot] = static_cast<uint16_t>(kValid | (*v & 0x3FFu)); any_valid = true; }
        else   { adbuf_[slot] = 0; }
    }
    return any_valid;
}

void Vr41xxPiuConverter::InvalidatePage(int page) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (uint16_t& b : page_buf_[page]) b &= ~kValid;
}

void Vr41xxPiuConverter::InvalidateAdBuffer() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (uint16_t& b : adbuf_) b &= ~kValid;
}

/* PIUABnREG holds ADPortScan data in AB0-3 and CMDScanDATA in AB0 (VR4121 UM Table 20-5,
   VR4102 UM Table 19-5). */
uint16_t Vr41xxPiuConverter::ReadHalf(uint32_t addr) {
    std::lock_guard<std::mutex> lk(mtx_);
    const uint32_t off = addr - MmioBase();
    if (off == kOffAb0 || off == kOffAb1 || off == kOffAb2 || off == kOffAb3) {
        return adbuf_[(off - kOffAb0) / 2u];
    }
    int page = 0, idx = 0;
    if (!PageBufferSlot(off, &page, &idx)) HaltUnsupportedAccess("VR41xx PIU2 ReadHalf", addr, 0);
    return page_buf_[page][idx];
}

/* PIUPBnmREG D15 VALID + D9:0 PADDATA are R/W, D14:10 RFU "write 0 / read 0" (VR4121 UM
   20.3.9, VR4102 UM 19.3.9); casio_toricomail_ce212 touch.dll sub_1370AB0 masks each page
   buffer to 10 bits and writes it back while recovering the coordinate. */
void Vr41xxPiuConverter::WriteHalf(uint32_t addr, uint16_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    int page = 0, idx = 0;
    if (!PageBufferSlot(addr - MmioBase(), &page, &idx)) {
        HaltUnsupportedAccess("VR41xx PIU2 WriteHalf", addr, value);
    }
    page_buf_[page][idx] = value & 0x83FFu;
}

void Vr41xxPiuConverter::Reset() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& pg : page_buf_) for (uint16_t& b : pg) b = 0;
    next_page_ = 0;
    for (uint16_t& b : adbuf_) b = 0;
}

void Vr41xxPiuConverter::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& pg : page_buf_) for (uint16_t b : pg) w.Write("pg", b);
    w.Write("next_page", next_page_);
    for (uint16_t b : adbuf_) w.Write("adbuf", b);
}

void Vr41xxPiuConverter::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& pg : page_buf_) for (uint16_t& b : pg) r.Read("pg", b);
    r.Read("next_page", next_page_);
    for (uint16_t& b : adbuf_) r.Read("adbuf", b);
}

}
