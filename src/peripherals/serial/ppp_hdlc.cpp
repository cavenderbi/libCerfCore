#include "ppp_hdlc.h"

#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/hdlc_fcs16.h>

#include <algorithm>

namespace cerfcore {

namespace {

constexpr uint8_t  kFlag    = 0x7E;
constexpr uint8_t  kEsc     = 0x7D;

}  /* namespace */

void PppHdlc::Feed(const uint8_t* data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const uint8_t b = data[i];
        if (b == kFlag) {
            if (in_frame_ && !in_escape_ && !rx_.empty()) EndFrame();
            rx_.clear();
            in_frame_  = true;
            in_escape_ = false;
            continue;
        }
        if (!in_frame_) continue;          /* inter-frame fill before a flag */
        if (b == kEsc) { in_escape_ = true; continue; }
        uint8_t v = b;
        if (in_escape_) { v = (uint8_t)(b ^ 0x20); in_escape_ = false; }
        rx_.push_back(v);
    }
}

void PppHdlc::EndFrame() {
    if (rx_.size() < 3) return;            /* proto(1, PFC) + FCS(2) minimum */
    if (HdlcFcs16::Update(HdlcFcs16::kInit, rx_.data(), rx_.size()) != HdlcFcs16::kGood) return;

    const uint8_t* p = rx_.data();
    size_t len = rx_.size() - 2;           /* drop the trailing FCS */

    if (len >= 2 && p[0] == 0xFF && p[1] == 0x03) { p += 2; len -= 2; }  /* ACFC */
    if (len < 1) return;

    uint16_t proto;
    if (p[0] & 0x01) { proto = p[0]; p += 1; len -= 1; }   /* PFC single octet */
    else {
        if (len < 2) return;
        proto = cerf::be::U16(p);
        p += 2; len -= 2;
    }
    if (sink_) sink_(proto, p, len);
}

void PppHdlc::BuildFrame(uint16_t protocol, const uint8_t* payload, size_t len,
                         std::vector<uint8_t>& out) const {
    std::vector<uint8_t> body(4 + len + 2);
    body[0] = 0xFF;
    body[1] = 0x03;
    cerf::be::Put16(body.data() + 2, protocol);
    std::copy(payload, payload + len, body.begin() + 4);

    const uint16_t fcs = (uint16_t)(HdlcFcs16::Update(HdlcFcs16::kInit, body.data(), 4 + len)
                                    ^ 0xFFFF);
    cerf::le::Put16(body.data() + 4 + len, fcs);

    out.push_back(kFlag);
    for (uint8_t b : body) {
        const bool esc = (b == kEsc) || (b == kFlag) ||
                         (b < 0x20 && (tx_accm_ & (1u << b)));
        if (esc) { out.push_back(kEsc); out.push_back((uint8_t)(b ^ 0x20)); }
        else      out.push_back(b);
    }
    out.push_back(kFlag);
}

}
