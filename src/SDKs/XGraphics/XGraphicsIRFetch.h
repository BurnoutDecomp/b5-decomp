#pragma once

// ===========================================================================
// XGRAPHICS::IRFetch -- one node kind in the XGRAPHICS shader-microcode
// intermediate representation (BURNOUT_X360_ARTIST.XEX). This header is the
// canonical OWNING home for:
//
//     XGRAPHICS::IRFetch::InstType  @ 0x82C2B8A0
//
// There is no Feb-2007 leak source and no DWARF for this TU. The single method
// is a leaf string accessor that returns the literal "fetch" -- the human-
// readable instruction-type tag for a texture/vertex fetch IR node. The class
// is declared minimally here (only what this TU defines); the full IRFetch node
// layout lives with the other XGRAPHICS IR TUs. `XGRAPHICS` is an X360 graphics-
// SDK boundary, so its identifiers are preserved verbatim per the naming
// convention.
// ===========================================================================

namespace XGRAPHICS
{

class IRFetch
{
public:
    // @ 0x82C2B8A0 -- returns the instruction-type tag string "fetch".
    const char* InstType();
};

} // namespace XGRAPHICS
