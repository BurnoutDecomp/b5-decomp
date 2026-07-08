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
// setFreeAnalyst plus this class's own TU (CVideoFrameAnalyst::Init / computeFrameTextureMap
// / detectHighTextureBand / the destructors). Byte offsets from this; stride 0x402C == 16428.
// The four leading words are heap buffers (X360 32-bit pointer values), allocated in Init and
// freed in the destructors; they are stored 32-bit exactly as the X360 `stw` writes them, so
// this whole record keeps X360 widths and the array stride stays byte-exact:
//   +0x00  muField00     analysis buffer 0 (XMemAlloc size = macro-block area bytes)
//   +0x04  muField04     analysis buffer 1 (XMemAlloc size = 4 * area)
//   +0x08  muField08     analysis buffer 2 (allocated by computeBlockAvg)
//   +0x0C  muField0C     analysis buffer 3 (XMemAlloc size = 4 * area)
//   +0x10  muSourceId    source-frame identity token (a pointer value), init 0xFFFFFFFF
//   +0x14  miAvailable   reuse flag: >0 = free for reuse, 0 = in use
//   +0x18 .. +0x4017  maActivityHistogram[4096]  per-value activity histogram (memset 0x4000
//                     bytes each frame; detectHighTextureBand slides a weighted window over it)
//   +0x4018  muArea            4 * ceil(w/16) * ceil(h/16)   (macro-block area)
//   +0x401C  muAreaHalf        muArea >> 1
//   +0x4020  muField4020       frame width  (a2 passed to Init)
//   +0x4024  muField4024       frame height (a3 passed to Init)
//   +0x4028  muWidthInBlocks   width >> 4
// =====================================================================================
#pragma once

#include <cstddef>

#include "types.hpp"

// Byte stride of one frame-analyst record (mulli r*, r*, 0x402C throughout the asm).
static const u32 KU_VIDEOFRAMEANALYST_STRIDE = 0x402C;

class CVideoFrameAnalyst
{
public:
    // Sets up (or resets) the analysis buffers for a width x height frame. Called once per
    // array element from CVideoAnalyst::Init. Returns the last allocation (vestigial).
    int Init(u32 uWidth, s32 iHeight);

    // Builds the texture-complexity map for the supplied source luma plane.
    void computeFrameTextureMap(u8* pSource);

    // Computes the per-block averages over the Y/U/V planes.
    int computeBlockAvg(u8* pY, u8* pU, u8* pV);

    // Scans maActivityHistogram for a contiguous high-activity band; on success writes the
    // band position to *pOutBand and returns 1, otherwise returns 0.
    int detectHighTextureBand(s32* pOutBand);

    // Frees the four analysis buffers. Non-virtual: this record carries no vptr (the four
    // leading words are heap buffers, not a vtable pointer).
    ~CVideoFrameAnalyst();

    // MSVC array ("vector") deleting destructor -- the helper the X360 build invokes to
    // tear down the CVideoAnalyst-owned frame array (flags: bit0 = free, bit1 = array).
    void* _vector_deleting_destructor_(u32 uFlags);

    // --- Members CVideoAnalyst manipulates directly (see file header) ----------------
    u32 muField00;      // +0x00  analysis buffer 0
    u32 muField04;      // +0x04  analysis buffer 1
    u32 muField08;      // +0x08  analysis buffer 2
    u32 muField0C;      // +0x0C  analysis buffer 3
    u32 muSourceId;     // +0x10  source-frame identity token
    s32 miAvailable;    // +0x14  reuse flag (>0 = free)

    s32 maActivityHistogram[0x1000];  // +0x18 .. +0x4017  per-value activity histogram

    u32 muArea;         // +0x4018  macro-block area (4 * blocksW * blocksH)
    u32 muAreaHalf;     // +0x401C  muArea >> 1
    u32 muField4020;    // +0x4020  frame width
    u32 muField4024;    // +0x4024  frame height
    u32 muWidthInBlocks;// +0x4028  width >> 4

    // Pin the byte layout: the array stride and the live fields must land exactly.
    static void _AssertLayout()
    {
        static_assert(offsetof(CVideoFrameAnalyst, muSourceId) == 0x10, "muSourceId @ +0x10");
        static_assert(offsetof(CVideoFrameAnalyst, miAvailable) == 0x14, "miAvailable @ +0x14");
        static_assert(offsetof(CVideoFrameAnalyst, maActivityHistogram) == 0x18, "histogram @ +0x18");
        static_assert(offsetof(CVideoFrameAnalyst, muArea) == 0x4018, "muArea @ +0x4018");
        static_assert(offsetof(CVideoFrameAnalyst, muAreaHalf) == 0x401C, "muAreaHalf @ +0x401C");
        static_assert(offsetof(CVideoFrameAnalyst, muField4020) == 0x4020, "muField4020 @ +0x4020");
        static_assert(offsetof(CVideoFrameAnalyst, muField4024) == 0x4024, "muField4024 @ +0x4024");
        static_assert(offsetof(CVideoFrameAnalyst, muWidthInBlocks) == 0x4028, "muWidthInBlocks @ +0x4028");
        static_assert(sizeof(CVideoFrameAnalyst) == 0x402C, "CVideoFrameAnalyst stride == 16428");
    }
};
