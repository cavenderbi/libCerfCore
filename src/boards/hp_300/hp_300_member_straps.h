#pragma once

#include <cerfcore/core/service.h>

namespace cerfcore {

class Hp300MemberStraps : public Service {
public:
    using Service::Service;

    virtual bool Luke() const   = 0;
    virtual bool Model2() const = 0;
};

template <bool kLuke, bool kModel2>
class Hp300FixedMemberStraps : public Hp300MemberStraps {
public:
    using Hp300MemberStraps::Hp300MemberStraps;

    bool Luke() const override   { return kLuke; }
    bool Model2() const override { return kModel2; }
};

}
