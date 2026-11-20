#pragma once

#include <cerfcore/core/service.h>
#include "../peripherals/touch/host_pen_target.h"
#include "../socs/sh77xx/sh77xx_intc.h"

namespace cerfcore {

class Sh77xxPortData;

class Sh77xxPenLinePanel : public Service, public HostPenTarget {
public:
    using Service::Service;

    void OnReady() override;

    void ApplyPen(const HostPenInput::Pen& pen) override;
    void ResetPen() override;

protected:
    virtual Sh77xxIntcPin PenLine() const = 0;

    void DrivePenLine(bool pen_down);
    bool PenLineHigh() const { return pen_down_; }

    Sh77xxPortData& Ports() const { return *ports_; }

private:
    Sh77xxIntc*     intc_     = nullptr;
    Sh77xxPortData* ports_    = nullptr;
    bool            pen_down_ = false;
};

}
