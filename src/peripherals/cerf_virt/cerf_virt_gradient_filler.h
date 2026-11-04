#pragma once

#include <cerfcore/peripherals/cerf_virt/cerf_virt_grad_descriptor.h>
#include "cerf_virt_blt_surface.h"
#include <cerfcore/core/service.h>

namespace cerfcore {

namespace CerfVirt {

class CerfVirtGradientFiller : public Service, public BltSurfaceAccess {
public:
    using Service::Service;
    bool ShouldRegister() override;
    void OnReady() override;

    bool Execute(const CerfGradDescriptor& g);

private:
    bool FillIndexed(const CerfGradDescriptor& g, uint32_t d_bits, int32_t span,
                     const int64_t base[4], const int64_t step[4]);

    BltIndexConverter index_converter_;
};

}

}
