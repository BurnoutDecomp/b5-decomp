// =====================================================================================
// CVideoFrameAnalyst -- per-frame analysis record for the RTCMV/WMV video-object
// encoder. CVideoAnalyst owns a heap array of these (one per in-flight frame) and drives
// them; the full analysis workspace (texture map, per-block averages) is this class's own
// business and is bodied by its own translation unit.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU. Only the members that
// CVideoAnalyst touches directly are named here; the remainder of the 16428-byte record
// is opaque workspace, preserved as padding so the array stride and the two live fields
// (+0x10 source id, +0x14 availability) land at their attested byte offsets. This is the
// minimal owning header -- the CVideoFrameAnalyst TU extends it additively.
//
// Layout attested by CVideoAnalyst::Init / analysisFrame / setCurFrameAnalyst /
// setFreeAnalyst (byte offsets from this; stride 0x402C == 16428):
//   +0x00 .. +0x0F   four u32 fields, zero-initialised per element
//   +0x10  muSourceId    source-frame identity token (a pointer value), init 0xFFFFFFFF
//   +0x14  miAvailable   reuse flag: >0 = free for reuse, 0 = in use
//   +0x18 .. +0x401F   opaque analysis workspace
//   +0x4020            u32 field, zero-initialised per element
//   +0x4024            u32 field, zero-initialised per element
//   +0x4028 .. +0x402B trailing padding to the 16428-byte stride
// =====================================================================================
#pragma once

#include <cstddef>

#include "types.hpp"

// Byte stride of one frame-analyst record (mulli r*, r*, 0x402C throughout the asm).
static const u32 KU_VIDEOFRAMEANALYST_STRIDE = 0x402C;

class CVideoFrameAnalyst
{
public:
    // Sets up the analysis buffers for a width x height frame. Called once per array
    // element from CVideoAnalyst::Init. (Bodied by this class's own TU.)
    int Init(u32 uWidth, s32 iHeight);

    // Builds the texture-complexity map for the supplied source luma plane.
    void computeFrameTextureMap(u8* pSource);

    // Computes the per-block averages over the Y/U/V planes.
    int computeBlockAvg(u8* pY, u8* pU, u8* pV);

    // MSVC array ("vector") deleting destructor -- the helper the X360 build invokes to
    // tear down the CVideoAnalyst-owned frame array (flags: bit0 = free, bit1 = array).
    void* _vector_deleting_destructor_(u32 uFlags);

    // --- Members CVideoAnalyst manipulates directly (see file header) ----------------
    u32 muField00;      // +0x00
    u32 muField04;      // +0x04
    u32 muField08;      // +0x08
    u32 muField0C;      // +0x0C
    u32 muSourceId;     // +0x10  source-frame identity token
    s32 miAvailable;    // +0x14  reuse flag (>0 = free)

private:
    u8  maWorkspace[0x4020 - 0x18];   // +0x18  opaque analysis workspace

public:
    u32 muField4020;    // +0x4020
    u32 muField4024;    // +0x4024

private:
    u8  maTailPad[0x402C - 0x4028];   // +0x4028  stride padding

    // Pin the byte layout: the array stride and the two live fields must land exactly.
    static void _AssertLayout()
    {
        static_assert(offsetof(CVideoFrameAnalyst, muSourceId) == 0x10, "muSourceId @ +0x10");
        static_assert(offsetof(CVideoFrameAnalyst, miAvailable) == 0x14, "miAvailable @ +0x14");
        static_assert(offsetof(CVideoFrameAnalyst, muField4020) == 0x4020, "muField4020 @ +0x4020");
        static_assert(offsetof(CVideoFrameAnalyst, muField4024) == 0x4024, "muField4024 @ +0x4024");
        static_assert(sizeof(CVideoFrameAnalyst) == 0x402C, "CVideoFrameAnalyst stride == 16428");
    }
};
