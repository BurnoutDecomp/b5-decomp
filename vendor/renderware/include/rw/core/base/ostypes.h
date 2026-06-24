#pragma once

// Canonical RenderWare SDK home for the global C-API colour type RwRGBAReal
// (EARenderWare rwcore, rw/core/base/ostypes.h:846-857). Reconstructed from
// BURNOUT_X360_ARTIST.XEX with the public-header names locked from the Feb-2007 leak
// (platform/include/native/ps3-ppu-gcc/rw/core/base/ostypes.h:956-969) and the DecFIGS
// DWARF (SDKs/EATech/include/cmn/rw/core/base/ostypes.h:846-857).
//
// This is the legacy RW3.x global C type (struct tag RwRGBARealTag + typedef), distinct
// from the PC RW4 rw::RGBA in the generated rwcore_structs.h (that is a 4-byte packed
// integer colour; this is four floating-point channels, 16 bytes). The PC rwcore.pdb does
// NOT expose RwRGBAReal (it lives only in the X360 rwcore.lib), so the data-driven
// generator cannot emit it -- hence this hand-maintained header. tools/renderware/generate_headers.py
// only writes rw/rwcore_enums.h / rw/rwcore_structs.h / rw/rwcore.h and never touches
// rw/core/, so (like the sibling rw/math/vpu/ headers) this file is immune to regeneration.
//
// RwReal is the RenderWare scalar (typedef float); modelled here as plain float so the
// header is self-contained and free of project primitives, matching the vendor convention
// used by rw/math/vpu/types.h.

// rw/core/base/ostypes.h:845 -- floating-point RGBA colour (red/green/blue/alpha in [0,1]).
struct RwRGBARealTag
{
    float red;     // ostypes.h:846
    float green;   // ostypes.h:847
    float blue;    // ostypes.h:848
    float alpha;   // ostypes.h:849

    // ostypes.h:851 -- channel-wise equality.
    bool operator==(const RwRGBARealTag &other) const
    {
        return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
    }
};

// ostypes.h:857
typedef RwRGBARealTag RwRGBAReal;
