# i.MX51 GPU RG88 color targets

The rasterizer accepts A2xx `COLORX_8_8` (format 4) as two 8-bit normalized
lanes in a 16-bit pixel. Resolve reads R/G and supplies B=0, A=1. Stores use
the existing 8-bit rounding/dither path and preserve masked lanes. Target
swap is handled by the existing raster path. Saved-state validation accepts
the same format set as drawing.

Primary format definitions:

- [Code Aurora COLORFORMATX enum](https://android.googlesource.com/kernel/msm/%2B/9bbf61339d31af416d12ae93b7a3544db547aa78/drivers/gpu/msm/a2xx_reg.h).
- [Driver surface-format and bytes-per-pixel tables](https://android.googlesource.com/kernel/msm.git/%2B/3f2bc4d6eb5a4fada842462ba22bb6bbb41d00c7/drivers/gpu/msm/adreno_drawctxt.c).

`imx51_gpu3d_color_test` covers all 65,536 unpacked RG88 words, lane masking,
missing channels, supported-format validation, and bit-exact resolve round-trips
for every 16-bit word. The frontend compiled and
all seven core CTests passed. The top-level build wrapper also attempted twelve
optional CE fixture apps, which remain unavailable because the host lacks their
toolchain/prebuilt executables; their failures do not describe the frontend build.

A stock SYNC2 boot previously stopped at `unsupported color format 4` twice
with identical guest register values. The initial RG88 build advanced to the
existing GMEM format/pitch/base-reinterpretation guard, with new color info
`0x00010004`. A diagnostic rerun measured previous binding `0x00000002`,
previous/current pitches 256/256, resolve=1, mask `0xF`, control `0x1C27`.
The supported exception is restricted to that RGB565 lower-bank binding and
RG88 upper-bank resolve view with unchanged 256-pixel pitch. Both views use
16-bit words. The resolve preserves the existing GMEM binding; arbitrary
rebinding remains rejected with the expanded diagnostic. Tests reject draw
rebinding, changed pitches, a different source pixel width and other bases.

This resembles a raw upper-bank/context save, possibly depth storage. That
purpose is an inference; the captured transition and lossless word codec do
not establish a complete save/restore implementation.

This does not establish arbitrary RG88 blend modes, GMEM rebinding, full GPU
conformance, or successful stock navigation startup. RG88 blending retains the
existing unsupported-format check. Full packet-level/rendered behavior needs
the recorded application reruns; codec tests alone are not that proof.


## Recorded application follow-up

The upper-bank build (`0af93641ff49b42ad660675e671450dc6218eb752642a330b18737bc15d4d172`)
passed system maintenance and reached the SYNC home screen by the 180-second
capture on the same prepared research card. The baseline format rejection and
initial RG88 binding rejection are retained separately in open-sync-maps
`docs/evidence/cold-entrance-ui/`. This establishes progress through those
startup gates; navigation search/routing acceptance is a separate check.
