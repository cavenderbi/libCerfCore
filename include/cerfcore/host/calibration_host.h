#pragma once

#include "../core/service.h"

#include <cstdint>

namespace cerfcore {

struct CalibrationAnswer {
    bool yes          = false;
    bool host_notices = true;
};

class CalibrationHost : public Service {
public:
    using Service::Service;

    virtual CalibrationAnswer ApprovesCalibration()                                    = 0;
    virtual CalibrationAnswer ApprovesPanelResolution(uint32_t width, uint32_t height) = 0;
    virtual void              CalibrationFinished()                                    = 0;
    virtual void              CalibrationFailed()                                      = 0;
    virtual void              GuestCalibrationUnhandled()                              = 0;
    virtual void              GuestCalibrationEnded(bool guest_pointer_restored)       = 0;
};

}
