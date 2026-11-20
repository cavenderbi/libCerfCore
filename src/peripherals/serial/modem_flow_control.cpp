#include "modem_flow_control.h"

#include "at_decimal.h"

#include <cerfcore/state/state_stream.h>

#include <cstdio>

namespace cerfcore {

namespace {

constexpr uint8_t kDc1 = 0x11u;
constexpr uint8_t kDc3 = 0x13u;

constexpr char kIfcRanges[] = "+IFC:(0-3),(0-2)";

}

void ModemFlowControl::LoadDefaults() { *this = ModemFlowControl{}; }

bool ModemFlowControl::SelectAmpK(unsigned k) {
    switch (k) {
        case 0u: Select(kDceByDteNone, kDteByDceNone); return true;
        case 3u: Select(kDceByDteRts, kDteByDceCts); return true;
        case 4u: Select(kDceByDteXonXoff, kDteByDceXonXoff); return true;
        case 5u: Select(kDceByDteXonXoffPass, kDteByDceXonXoff); return true;
        default: return false;
    }
}

bool ModemFlowControl::ExecuteIfc(const std::string& body, size_t& i, std::string& info) {
    if (i < body.size() && body[i] == '?') {
        ++i;
        char text[16];
        std::snprintf(text, sizeof text, "+IFC:%u,%u", unsigned{dce_by_dte_}, unsigned{dte_by_dce_});
        info = text;
        return true;
    }
    if (i >= body.size() || body[i] != '=') return false;
    ++i;
    if (i < body.size() && body[i] == '?') {
        ++i;
        info = kIfcRanges;
        return true;
    }
    unsigned dce_by_dte = 0u;
    unsigned dte_by_dce = 0u;
    if (!ReadAtValue(body, i, kDceByDteXonXoffPass, dce_by_dte)) return false;
    if (i < body.size() && body[i] == ',') {
        ++i;
        if (!ReadAtValue(body, i, kDteByDceCts, dte_by_dce)) return false;
    }
    if (i < body.size() && body[i] == ',') return false;
    Select(static_cast<uint8_t>(dce_by_dte), static_cast<uint8_t>(dte_by_dce));
    return true;
}

void ModemFlowControl::Select(uint8_t dce_by_dte, uint8_t dte_by_dce) {
    dce_by_dte_ = dce_by_dte;
    dte_by_dce_ = dte_by_dce;
}

void ModemFlowControl::EnterDataState() { xoff_ = false; }

bool ModemFlowControl::SoftwareFlow() const {
    return dce_by_dte_ == kDceByDteXonXoff || dce_by_dte_ == kDceByDteXonXoffPass;
}

bool ModemFlowControl::TakeGuestData(const uint8_t* data, size_t n, std::vector<uint8_t>& passed) {
    const bool software = SoftwareFlow();
    const bool strip    = dce_by_dte_ == kDceByDteXonXoff;
    bool       moved    = false;
    passed.reserve(passed.size() + n);
    for (size_t i = 0; i < n; ++i) {
        const uint8_t b = data[i];
        if (software && (b == kDc1 || b == kDc3)) {
            xoff_ = b == kDc3;
            moved = true;
            if (strip) continue;
        }
        passed.push_back(b);
    }
    return moved;
}

bool ModemFlowControl::ReceiveReady(bool rts) const {
    if (dce_by_dte_ == kDceByDteNone) return true;
    if (dce_by_dte_ == kDceByDteRts) return rts;
    return !xoff_;
}

void ModemFlowControl::SaveState(StateWriter& w) const {
    w.Write("modem_ifc_dce_by_dte", dce_by_dte_);
    w.Write("modem_ifc_dte_by_dce", dte_by_dce_);
    w.Write<uint8_t>("modem_xoff", xoff_ ? 1u : 0u);
}

void ModemFlowControl::RestoreState(StateReader& r) {
    uint8_t xoff = 0u;
    r.Read("modem_ifc_dce_by_dte", dce_by_dte_);
    r.Read("modem_ifc_dte_by_dce", dte_by_dce_);
    r.Read("modem_xoff", xoff);
    xoff_ = xoff != 0u;
}

}
