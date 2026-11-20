#include "sh77xx_scif_fifo.h"

#include <algorithm>

namespace cerfcore {

namespace {

constexpr uint8_t kStatusTend      = 0x40u;
constexpr uint8_t kStatusTdfe      = 0x20u;
constexpr uint8_t kStatusRdf       = 0x02u;
constexpr uint8_t kStatusDr        = 0x01u;
constexpr uint8_t kStatusClearable = 0xF3u;

constexpr size_t kFifoBytes     = 16u;
constexpr size_t kTxTriggers[4] = {8u, 4u, 2u, 1u};
constexpr size_t kRxTriggers[4] = {1u, 4u, 8u, 14u};

size_t TxTrigger(uint8_t fcr) { return kTxTriggers[(fcr >> 4) & 3u]; }

size_t RxTrigger(uint8_t fcr) { return kRxTriggers[(fcr >> 6) & 3u]; }

}

bool Sh77xxScifFifo::Reset() {
    const bool had_rx = !RxEmpty();
    read_ones_ = 0u;
    tend_      = true;
    tdfe_      = true;
    rdf_       = false;
    dr_        = false;
    tx_.clear();
    rx_.clear();
    rx_pos_ = 0u;
    return had_rx;
}

uint16_t Sh77xxScifFifo::DataCounts() const { return static_cast<uint16_t>((tx_.size() << 8) | RxCount()); }

uint8_t Sh77xxScifFifo::ReadStatus(bool mark_read) {
    const uint8_t v = static_cast<uint8_t>((tend_ ? kStatusTend : 0u) | (tdfe_ ? kStatusTdfe : 0u) |
                                           (rdf_ ? kStatusRdf : 0u) | (dr_ ? kStatusDr : 0u));
    if (mark_read) read_ones_ = static_cast<uint8_t>(read_ones_ | v);
    return v;
}

void Sh77xxScifFifo::WriteStatus(uint16_t value, uint8_t fcr) {
    const uint8_t cleared = static_cast<uint8_t>(~value & read_ones_ & kStatusClearable);
    if ((cleared & kStatusTend) != 0u) tend_ = false;
    if ((cleared & kStatusTdfe) != 0u && tx_.size() > TxTrigger(fcr)) tdfe_ = false;
    if ((cleared & kStatusRdf) != 0u && RxCount() < RxTrigger(fcr)) rdf_ = false;
    if ((cleared & kStatusDr) != 0u) dr_ = false;
    read_ones_ = static_cast<uint8_t>(read_ones_ & ~cleared);
    UpdateRxFlags(fcr);
}

bool Sh77xxScifFifo::PushTransmit(uint8_t value) {
    if (tx_.size() >= kFifoBytes) return false;
    tx_.push_back(value);
    tend_      = false;
    read_ones_ = static_cast<uint8_t>(read_ones_ & ~kStatusTend);
    return true;
}

void Sh77xxScifFifo::TakeTransmit(std::vector<uint8_t>& out) {
    if (tx_.empty()) return;
    out.swap(tx_);
    tend_ = true;
    tdfe_ = true;
}

void Sh77xxScifFifo::EndTransmit() { tend_ = true; }

void Sh77xxScifFifo::ClearTransmit() { tx_.clear(); }

bool Sh77xxScifFifo::ClearReceive() {
    const bool had_rx = !RxEmpty();
    rx_.clear();
    rx_pos_ = 0u;
    return had_rx;
}

bool Sh77xxScifFifo::RxEmpty() const { return rx_pos_ == rx_.size(); }

bool Sh77xxScifFifo::RxFull() const { return RxCount() == kFifoBytes; }

uint8_t Sh77xxScifFifo::PopReceive(uint8_t fcr, bool& drained) {
    const uint8_t value = rx_[rx_pos_++];
    drained             = rx_pos_ == rx_.size();
    if (drained) {
        rx_.clear();
        rx_pos_ = 0u;
    }
    UpdateRxFlags(fcr);
    return value;
}

void Sh77xxScifFifo::PushReceive(const uint8_t* data, size_t n, uint8_t fcr) {
    rx_.insert(rx_.end(), data, data + n);
    UpdateRxFlags(fcr);
}

size_t Sh77xxScifFifo::RxCount() const { return std::min(rx_.size() - rx_pos_, kFifoBytes); }

void Sh77xxScifFifo::UpdateRxFlags(uint8_t fcr) {
    const size_t count = RxCount();
    if (count >= RxTrigger(fcr)) rdf_ = true;
    else if (count > 0u) dr_ = true;
}

void Sh77xxScifFifo::SaveState(StateWriter& w) const {
    w.Write("read_ones", read_ones_);
    w.Write("tend", tend_);
    w.Write("tdfe", tdfe_);
    w.Write("rdf", rdf_);
    w.Write("dr", dr_);
    w.Write<uint64_t>("tx_count", tx_.size());
    if (!tx_.empty()) w.WriteBytes("tx", tx_.data(), tx_.size());
    w.Write<uint64_t>("rx_count", rx_.size() - rx_pos_);
    if (!RxEmpty()) w.WriteBytes("rx", rx_.data() + rx_pos_, rx_.size() - rx_pos_);
}

void Sh77xxScifFifo::RestoreState(StateReader& r) {
    r.Read("read_ones", read_ones_);
    r.Read("tend", tend_);
    r.Read("tdfe", tdfe_);
    r.Read("rdf", rdf_);
    r.Read("dr", dr_);
    uint64_t tx_count = 0u;
    r.Read("tx_count", tx_count);
    tx_.assign(static_cast<size_t>(tx_count), 0u);
    if (!tx_.empty()) r.ReadBytes("tx", tx_.data(), tx_.size());
    uint64_t rx_count = 0u;
    r.Read("rx_count", rx_count);
    rx_.assign(static_cast<size_t>(rx_count), 0u);
    rx_pos_ = 0u;
    if (!rx_.empty()) r.ReadBytes("rx", rx_.data(), rx_.size());
}

}
