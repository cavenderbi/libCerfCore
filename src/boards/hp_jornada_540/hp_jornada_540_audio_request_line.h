#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class HpJornada540AudioRequestLine : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    bool Asserted() const { return asserted_; }
    bool Drive(bool asserted);

private:
    bool asserted_ = false;
};

}
