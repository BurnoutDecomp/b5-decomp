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
// Layout (0x94 = 148 bytes). Only the fields that CReferenceLibrary::SetupReferenceLibrary
// attests are named here: five CRct regions initialised to the empty rect {0,0,-1,-1} at
// +0x00 / +0x10 / +0x48 / +0x60 / +0x78. The remaining bytes are filled by
// PictureCYUV420_init and are preserved as opaque padding until that TU recovers them.
//   +0x00 mrcRegion0   CRct  (empty-initialised at construct)
//   +0x10 mrcRegion1   CRct  (empty-initialised)
//   +0x20 ..+0x47      init-owned fields (padding)
//   +0x48 mrcRegion2   CRct  (empty-initialised)
//   +0x58 ..+0x5F      init-owned fields (padding)
//   +0x60 mrcRegion3   CRct  (empty-initialised)
//   +0x70 ..+0x77      init-owned fields (padding)
//   +0x78 mrcRegion4   CRct  (empty-initialised)
//   +0x88 ..+0x93      init-owned fields (padding)
// =====================================================================================
#pragma once

#include <cstddef>

#include "types.hpp"
#include "SDKs/EATech/rwmovie/CIntImage.h"   // CRct (its real home)

struct PictureCYUV420
{
    CRct mrcRegion0;   // +0x00
    CRct mrcRegion1;   // +0x10
    u8   mPad0[40];    // +0x20 .. +0x47  (filled by PictureCYUV420_init)
    CRct mrcRegion2;   // +0x48
    u8   mPad1[8];     // +0x58 .. +0x5F
    CRct mrcRegion3;   // +0x60
    u8   mPad2[8];     // +0x70 .. +0x77
    CRct mrcRegion4;   // +0x78
    u8   mPad3[12];    // +0x88 .. +0x93

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
        static_assert(offsetof(PictureCYUV420, mrcRegion1) == 0x10, "mrcRegion1 @ +0x10");
        static_assert(offsetof(PictureCYUV420, mrcRegion2) == 0x48, "mrcRegion2 @ +0x48");
        static_assert(offsetof(PictureCYUV420, mrcRegion3) == 0x60, "mrcRegion3 @ +0x60");
        static_assert(offsetof(PictureCYUV420, mrcRegion4) == 0x78, "mrcRegion4 @ +0x78");
        static_assert(sizeof(PictureCYUV420) == 0x94, "PictureCYUV420 size == 148");
    }
};

// Free-function fill / teardown for a PictureCYUV420 (each bodied by its own TU). init()
// populates the picture from the caller's parameters, writing a status code back through
// lpiResult (0 on success); Clean() releases whatever init() allocated. The parameter set
// after lpiResult is forwarded verbatim by CReferenceLibrary::SetupReferenceLibrary.
extern void PictureCYUV420_init(PictureCYUV420* lpPicture, s32* lpiResult,
                                s32 liParam2, s32 liParam3, s32 liParam4, s32 liParam5, s32 liParam6);
extern void PictureCYUV420_Clean(PictureCYUV420* lpPicture);
