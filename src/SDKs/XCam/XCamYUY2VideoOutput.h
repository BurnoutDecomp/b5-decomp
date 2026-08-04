#pragma once

// ===========================================================================
// XCAM::CYUY2VideoOutput -- the YUY2 (packed YUV 4:2:2) specialisation of the
// XCAM (X360 camera/capture) SDK video-output family in BURNOUT_X360_ARTIST.XEX.
// It derives from XCAM::CVideoOutputBase (see XCamVideoOutput.h). Unlike the
// planar CI420VideoOutput, YUY2 is a SINGLE packed plane, so the derived class
// adds no data members: the whole frame lives in the base's plane-0 ring
// (mpBufferResource[3] / mBuffers[3]) and sizeof(CYUY2VideoOutput) equals
// sizeof(CVideoOutputBase) == 0xB0. This is attested by XCAM::CEncoder, which
// embeds a CYUY2VideoOutput by value at +0x08 with its next member landing at
// +0xB8 (= 0x08 + 0xB0).
//
// This header is the canonical OWNING home for CYUY2VideoOutput. Only the
// methods reached by already-reconstructed callers (and the sibling methods
// they chain to) are declared here; the remaining YUY2 methods are homed by
// their own TUs as they are reconstructed. There is no reference source and no
// DWARF for this TU; the shape is reconstructed from the X360 asm. `XCAM` is an
// X360 SDK boundary, so its identifiers are preserved verbatim per the naming
// convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamVideoOutput.h"

namespace XCAM
{

// Texture format word baked into the YUY2 CreateTexture calls (`lis r8,0x1A20 ;
// ori r8,r8,0xB` -> 0x1A20000B): the tiled packed 16-bit YUY2 (YUV 4:2:2) plane
// format. Full-res (single packed plane), unlike the I420 8-bit planar format.
static const u32 KU_XCAM_YUY2_PLANE_FORMAT = 0x1A20000Bu;

// D3DType argument to CreateTexture (`li r10,3`).
static const u32 KU_XCAM_YUY2_TEXTURE_TYPE = 3u;

// YUY2 pixel-shader microcode blob (dword_8210C098) handed to
// D3DDevice_CreatePixelShader. The image holds it in full -- a 0x288-byte X360
// GPU shader container in .rdata at 0x8210C098 (first dword 0x102A1100), bounded
// by flt_8210C320 at 0x8210C320 (measured, headless IDA dump 2026-08-04). It is
// X360 Xenos microcode with no meaning to the PC target, so it is deliberately
// kept extern-only: declared and NOT defined.
extern const u8 gXCamYUY2PixelShaderCode[];

class CYUY2VideoOutput : public CVideoOutputBase
{
public:
    // @ 0x82985C80 -- forward (device,width,height) to the base, then create the
    // single packed YUY2 ring plane (a D3D texture per ring slot when a device is
    // present, else a raw XMem block), seed the descriptors and Reset. Returns 0,
    // or 8 on any resource-allocation failure (KI_XCAM_VIDEO_OUT_OF_RESOURCES).
    // Reached by XCAM::CEncoder::Initialize.
    int Initialize(D3DDevice* pDevice, s32 iWidth, s32 iHeight);

    // @ 0x82984A50 -- bind the quad + the ready frame's packed plane texture and
    // draw the screen-aligned quad, then unbind everything.
    void Render();

    // @ 0x82984628 -- consume the ready frame under the base helper and copy its
    // packed YUY2 plane into the caller's buffer. Returns 0, or 997 if no frame
    // is ready.
    int GetNextBuffer(SVideoBuffer* pDst);

    // @ own TU -- clear the ring slots and re-arm the producer cursor. Declared
    // here (declaration only) so Initialize can chain to it; the body is
    // reconstructed by its own TU.
    void Reset();

    // @ 0x82984BD0 -- consume the ready frame under the base helper and software-
    // convert its packed YUY2 plane to 32-bit ARGB into the caller's buffer
    // (pDst = {pitch,data}, one u32 per pixel). Returns 0, or 997 if no frame is
    // ready. The VMX128 body is a per-macropixel BT.601 matrix multiply; every
    // rodata constant it consumes was dumped from the image on 2026-08-04
    // (scratchpad/waveM/YUY2Output.spec.md holds the full table): the XDK-rounded
    // coefficients 1.164/1.596/-0.813/-0.391/2.018 (flt_8210C330/320/324/328/32C),
    // the normalisation constants 1/255, 0.0625, 0.5 (flt_82010C1C / flt_82046E00
    // / flt_82001DA0), the output scale 255.0 (flt_82010C20), and the 32-byte
    // [0,1] clamp pair unk_8210C300. The same rodata is shared verbatim by
    // XCAM::CI420VideoOutput::GetNextBufferRGB @ 0x82985360 (xref-attested).
    int GetNextBufferRGB(SVideoBuffer* pDst);

    // No additional data members -- the packed YUY2 plane reuses the base ring.
};

} // namespace XCAM
