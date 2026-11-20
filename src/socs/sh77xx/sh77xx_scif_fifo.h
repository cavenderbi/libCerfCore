#pragma once

#include <cerfcore/state/state_stream.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cerfcore {

class Sh77xxScifFifo {
public:
    bool     Reset();
    uint16_t DataCounts() const;
    uint8_t  ReadStatus(bool mark_read);
    void     WriteStatus(uint16_t value, uint8_t fcr);
    bool     PushTransmit(uint8_t value);
    void     TakeTransmit(std::vector<uint8_t>& out);
    void     EndTransmit();
    void     ClearTransmit();
    bool     ClearReceive();
    bool     RxEmpty() const;
    bool     RxFull() const;
    bool     TxHeld() const { return !tx_.empty(); }
    uint8_t  PopReceive(uint8_t fcr, bool& drained);
    void     PushReceive(const uint8_t* data, size_t n, uint8_t fcr);
    bool     TransmitRequest() const { return tdfe_; }
    bool     ReceiveRequest() const { return rdf_ || dr_; }

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    size_t RxCount() const;
    void   UpdateRxFlags(uint8_t fcr);

    uint8_t              read_ones_ = 0u;
    bool                 tend_      = true;
    bool                 tdfe_      = true;
    bool                 rdf_       = false;
    bool                 dr_        = false;
    std::vector<uint8_t> tx_;
    std::vector<uint8_t> rx_;
    size_t               rx_pos_ = 0u;
};

}
