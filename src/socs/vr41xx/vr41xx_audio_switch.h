#pragma once

#include <cerfcore/core/service.h>
#include "vr41xx_giu.h"

#include <functional>

namespace cerfcore {

class Vr41xxAudioSwitch : public Service {
public:
    using Service::Service;

    virtual bool SpeakerOn()                                      = 0;
    virtual bool MicOn()                                          = 0;
    virtual void RegisterChangeListener(std::function<void()> fn) = 0;
};

class Vr41xxGpioAudioSwitch : public Vr41xxAudioSwitch {
public:
    using Vr41xxAudioSwitch::Vr41xxAudioSwitch;

    bool SpeakerOn() override;
    void RegisterChangeListener(std::function<void()> fn) override;

protected:
    bool PinAt(int gpio, Vr41xxPinDrive level);

    virtual int            SpeakerGpio() const    = 0;
    virtual Vr41xxPinDrive SpeakerOnLevel() const = 0;
};

}
