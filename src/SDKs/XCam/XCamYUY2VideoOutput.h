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
// method(s) reached by already-reconstructed callers are declared here; the
// remaining YUY2 methods are homed by their own TUs as they are reconstructed.
// There is no reference source and no DWARF for this TU; the shape is
// reconstructed from the X360 asm. `XCAM` is an X360 SDK boundary, so its
// identifiers are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamVideoOutput.h"

namespace XCAM
{

class CYUY2VideoOutput : public CVideoOutputBase
{
public:
    // @ 0x82985C?? -- forward (device,width,height) to the base and set up the
    // single packed YUY2 ring plane. Returns 0 on success, or 8 on any
    // resource-allocation failure. Reached by XCAM::CEncoder::Initialize; the
    // body is reconstructed by this method's own TU.
    int Initialize(D3DDevice* pDevice, s32 iWidth, s32 iHeight);

    // No additional data members -- the packed YUY2 plane reuses the base ring.
};

} // namespace XCAM
