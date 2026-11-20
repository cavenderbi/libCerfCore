#include "casio_jx670_socket.h"

#include "casio_jx670_guest_writes.h"
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kOffStrap8000      = 0x8000u;
constexpr uint32_t kOffSocketId8010   = 0x8010u;
constexpr uint32_t kOffSocket8014     = 0x8014u;
constexpr uint32_t kOffCfg8018        = 0x8018u;
constexpr uint32_t kOffSocketCtrlAC8  = 0x0AC8u;
constexpr uint32_t kOffCardDetectA008 = 0xA008u;
constexpr uint32_t kOffSocketA038     = 0xA038u;
constexpr uint32_t kOffSocketStatus   = 0xA03Cu;

constexpr uint32_t kSocketIdEmpty   = 0x1Fu;
constexpr uint32_t kCardDetectEmpty = 0x2u;

constexpr uint32_t kStrap8000Written  = 0xFF1Fu;
constexpr uint32_t kSocket8014Written = 0x0001u;
constexpr uint32_t kCtrlAC8Written    = 0x003Cu;

}

void CasioJx670Socket::PowerOnRegisters() {
    strap8000_ = socket_8014_ = cfg_8018_ = socket_ctrl_ac8_ = socket_a038_ = 0;
}

bool CasioJx670Socket::TryReadHalf(uint32_t off, uint16_t& out) const {
    switch (off) {
        case kOffStrap8000:    out = static_cast<uint16_t>(strap8000_); return true;
        case kOffCfg8018:      out = static_cast<uint16_t>(cfg_8018_);  return true;
        case kOffSocketId8010: out = kSocketIdEmpty;                     return true;
        default:               return false;
    }
}

bool CasioJx670Socket::TryReadWord(uint32_t off, uint32_t& out) const {
    switch (off) {
        case kOffSocketStatus:   out = 0u;               return true;
        case kOffSocketCtrlAC8:  out = socket_ctrl_ac8_; return true;
        case kOffSocketA038:     out = socket_a038_;     return true;
        case kOffSocket8014:     out = socket_8014_;     return true;
        case kOffSocketId8010:   out = kSocketIdEmpty;   return true;
        case kOffCardDetectA008: out = kCardDetectEmpty; return true;
        default:                 return false;
    }
}

bool CasioJx670Socket::TryWriteHalf(uint32_t off, uint16_t value) {
    switch (off) {
        case kOffStrap8000:
            if (!SetsOnly(value, kStrap8000Written)) return false;
            strap8000_ = value;
            return true;
        case kOffCfg8018:
            if (value != 0u && value != cfg_8018_) return false;
            cfg_8018_ = value;
            return true;
        default:
            return false;
    }
}

bool CasioJx670Socket::TryWriteWord(uint32_t off, uint32_t value) {
    switch (off) {
        case kOffSocketCtrlAC8:
            if (!SetsOnly(value, kCtrlAC8Written)) return false;
            socket_ctrl_ac8_ = value;
            return true;
        case kOffSocketA038:
            if (!SetsOnly(value, socket_a038_)) return false;
            socket_a038_ = value;
            return true;
        case kOffSocket8014:
            if (!SetsOnly(value, kSocket8014Written)) return false;
            socket_8014_ = value;
            return true;
        case kOffStrap8000:
            if (!SetsOnly(value, kStrap8000Written)) return false;
            strap8000_ = value;
            return true;
        default:
            return false;
    }
}

void CasioJx670Socket::SaveState(StateWriter& w) const {
    w.Write("strap8000", strap8000_);
    w.Write("socket_8014", socket_8014_);
    w.Write("cfg_8018", cfg_8018_);
    w.Write("socket_ctrl_ac8", socket_ctrl_ac8_);
    w.Write("socket_a038", socket_a038_);
}

void CasioJx670Socket::RestoreState(StateReader& r) {
    r.Read("strap8000", strap8000_);
    r.Read("socket_8014", socket_8014_);
    r.Read("cfg_8018", cfg_8018_);
    r.Read("socket_ctrl_ac8", socket_ctrl_ac8_);
    r.Read("socket_a038", socket_a038_);
}

}
