#pragma once

#include <cerfcore/core/service.h>

#include <functional>

namespace cerfcore {

struct Sh77xxScifChannel;

class Sh77xxScifPins : public Service {
public:
    using Service::Service;

    virtual bool TxdLow(const Sh77xxScifChannel& channel) const                                = 0;
    virtual bool CtsHigh(const Sh77xxScifChannel& channel) const                               = 0;
    virtual void PublishEnables(const Sh77xxScifChannel& channel, bool transmit, bool receive) = 0;
    virtual void DriveRts(const Sh77xxScifChannel& channel, bool high)                         = 0;
    virtual void AddPinListener(std::function<void()> listener)                               = 0;
};

}
