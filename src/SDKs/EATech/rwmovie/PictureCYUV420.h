// =====================================================================================
// PictureCYUV420 -- reference-frame picture record held by CReferenceLibrary's frame
// queue for the RTCMV/WMV video-object encoder. Raw-allocated (XMemAlloc, 0x94 bytes) and
// two-phase constructed: a default construct that empties its CRct regions, followed by the
// PictureCYUV420_init() fill. This is a DISTINCT type from CPictureFYUV420 (which is 0x44
// bytes); do not conflate them.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No reference
// source and no DecFIGS DWARF hints exist for this TU.
//
// Layout (0x94 = 148 bytes) and allocation ownership recovered from ARTIST
// PictureCYUV420_init 82AA0420, Plane_allocate 82AA0340, and Clean 82AA02B0.
// Five CRct values are initialized by the caller; three plane views and their raw
// allocations occupy the gaps between the last three rectangles.
// =====================================================================================
#pragma once

#include <cstddef>

#include "types.hpp"
#include "SDKs/EATech/rwmovie/CIntImage.h"   // CRct (its real home)

struct PictureCYUV420
{
    CRct mrcRegion0;   // +0x00
    CRct mrcRegion1;   // +0x10
    s32 miLumaArea;       // +0x20
    s32 miChromaArea;     // +0x24
    s32 miLumaWidth;      // +0x28
    s32 miLumaHeight;     // +0x2C
    s32 miChromaWidth;    // +0x30
    s32 miChromaHeight;   // +0x34
    s32 miDisplayWidth;   // +0x38
    s32 miDisplayHeight;  // +0x3C
    u8* mpLuma;           // +0x40 aligned plane
    void* mpLumaAllocation; // +0x44 raw allocation
    CRct mrcRegion2;       // +0x48 luma allocation rectangle
    u8* mpChromaU;         // +0x58
    void* mpChromaUAllocation; // +0x5C
    CRct mrcRegion3;       // +0x60 chroma U allocation rectangle
    u8* mpChromaV;         // +0x70
    void* mpChromaVAllocation; // +0x74
    CRct mrcRegion4;       // +0x78 chroma V allocation rectangle
    u8* mapPlanes[3];      // +0x88 luma, chroma U, chroma V


    // The X360 performs exactly these stores inline (each region's four words set to
    // {0,0,-1,-1}) in SetupReferenceLibrary before calling PictureCYUV420_init; no other
    // field is touched at construction, so the padding is deliberately left uninitialised.
    PictureCYUV420()
    {
        mrcRegion0.miLeft = 0; mrcRegion0.miTop = 0; mrcRegion0.miRight = -1; mrcRegion0.miBottom = -1;
        mrcRegion1.miLeft = 0; mrcRegion1.miTop = 0; mrcRegion1.miRight = -1; mrcRegion1.miBottom = -1;
        mrcRegion2.miLeft = 0; mrcRegion2.miTop = 0; mrcRegion2.miRight = -1; mrcRegion2.miBottom = -1;
        mrcRegion3.miLeft = 0; mrcRegion3.miTop = 0; mrcRegion3.miRight = -1; mrcRegion3.miBottom = -1;
        mrcRegion4.miLeft = 0; mrcRegion4.miTop = 0; mrcRegion4.miRight = -1; mrcRegion4.miBottom = -1;
    }

    // Pin the recovered region offsets and the 148-byte console size.
    static void _AssertLayout()
    {
        CGS_STATIC_ASSERT((offsetof(PictureCYUV420, mrcRegion1) == 0x10), "mrcRegion1 @ +0x10");
        CGS_STATIC_ASSERT((offsetof(PictureCYUV420, mrcRegion2) == 0x48), "mrcRegion2 @ +0x48");
        CGS_STATIC_ASSERT((offsetof(PictureCYUV420, mrcRegion3) == 0x60), "mrcRegion3 @ +0x60");
        CGS_STATIC_ASSERT((offsetof(PictureCYUV420, mrcRegion4) == 0x78), "mrcRegion4 @ +0x78");
        CGS_STATIC_ASSERT((offsetof(PictureCYUV420, mpLuma) == 0x40), "luma plane");
        CGS_STATIC_ASSERT((offsetof(PictureCYUV420, mpChromaU) == 0x58), "chroma U plane");
        CGS_STATIC_ASSERT((offsetof(PictureCYUV420, mpChromaV) == 0x70), "chroma V plane");
        CGS_STATIC_ASSERT((offsetof(PictureCYUV420, mapPlanes) == 0x88), "plane map");
        CGS_STATIC_ASSERT((sizeof(PictureCYUV420) == 0x94), "PictureCYUV420 size == 148");
    }
};

// Free-function fill / teardown for a PictureCYUV420 (each bodied by its own TU). init()
// populates the picture from the caller's parameters, writing a status code back through
// lpiResult (0 on success); Clean() releases whatever init() allocated. The parameter set
// after lpiResult is forwarded verbatim by CReferenceLibrary::SetupReferenceLibrary.
extern void PictureCYUV420_init(PictureCYUV420* lpPicture, s32* lpiResult,
                                const CRct* lpLumaRect, const CRct* lpChromaRect,
                                s32 liParam4, s32 liParam5, s32 liParam6);
extern void PictureCYUV420_Clean(PictureCYUV420* lpPicture);
