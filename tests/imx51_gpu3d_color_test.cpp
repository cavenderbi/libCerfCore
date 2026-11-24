#include "../src/socs/imx51/imx51_gpu3d_color.h"

#include <cmath>
#include <cstdio>

int main() {
    int failures = 0;
    const auto check = [&](bool ok, const char* name) {
        if (!ok) { std::printf("FAIL %s\n", name); ++failures; }
    };
    using namespace cerfcore;
    check(Imx51Gpu3dSupportedColorFormat(4), "RG88 valid for draw and saved-state binding");
    check(!Imx51Gpu3dSupportedColorFormat(3) && !Imx51Gpu3dSupportedColorFormat(15), "other formats remain unsupported");
    check(Imx51Gpu3dRg88UpperBankResolve(2,256,0x10004,256,true), "captured upper-bank resolve");
    check(!Imx51Gpu3dRg88UpperBankResolve(2,256,0x10004,256,false), "draw rebinding remains unsupported");
    check(!Imx51Gpu3dRg88UpperBankResolve(5,256,0x10004,256,true), "different pixel width rejected");
    check(!Imx51Gpu3dRg88UpperBankResolve(2,128,0x10004,256,true), "changed source pitch rejected");
    check(!Imx51Gpu3dRg88UpperBankResolve(2,256,0x10004,128,true), "changed target pitch rejected");
    check(!Imx51Gpu3dRg88UpperBankResolve(2,256,4,256,true), "unmeasured base view rejected");
    check(Imx51Gpu3dPackRg88(0x12,0x34,0xABCD,15) == 0x3412, "two lanes, little-endian word");
    check(Imx51Gpu3dPackRg88(0x12,0x34,0xABCD,1) == 0xAB12, "red-only mask preserves green");
    check(Imx51Gpu3dPackRg88(0x12,0x34,0xABCD,2) == 0x34CD, "green-only mask preserves red");
    check(Imx51Gpu3dPackRg88(0x12,0x34,0xABCD,12) == 0xABCD, "unstored blue/alpha mask");
    for (unsigned red = 0; red < 256; ++red) {
        for (unsigned green = 0; green < 256; ++green) {
            const auto color = Imx51Gpu3dUnpackRg88(static_cast<uint16_t>(red | (green << 8)));
            check(std::abs(color[0] - red/255.0f) < 0.000001f &&
                  std::abs(color[1] - green/255.0f) < 0.000001f &&
                  color[2] == 0.0f && color[3] == 1.0f, "normalized load and missing lanes");
            check(Imx51Gpu3dPackRg88(static_cast<uint8_t>(color[0]*255u),
                static_cast<uint8_t>(color[1]*255u),0,15) == (red | (green << 8)),
                "resolve preserves all 16-bit memory words");
        }
    }
    return failures ? 1 : 0;
}
