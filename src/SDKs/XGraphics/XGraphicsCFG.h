#pragma once

// ===========================================================================
// XGRAPHICS::CFG -- the shader-microcode compiler's control-flow-graph / build
// context in BURNOUT_X360_ARTIST.XEX. It owns the compiler XGRAPHICS::Arena and
// hands out sequential virtual-register ids as XGRAPHICS::VRegInfo nodes are
// created (see VRegInfo::VRegInfo, CFG::BuildUsesAndDefs). It is the `a4`
// context threaded through every value-kind constructor.
//
// PARTIAL / SEED reconstruction: only the two members XGRAPHICS::VRegInfo reads
// are attested here (from the VRegInfo ctor asm). The full CFG -- with
// CFG::BuildUsesAndDefs, CFG::AddToRootSet, etc. -- is a separate TU; EXTEND
// this header there, do NOT fork it. `XGRAPHICS` is an X360 graphics-SDK
// boundary, so its identifiers are preserved verbatim per the naming convention.
//
// ATTESTED members (X360 byte displacements from VRegInfo::VRegInfo):
//   +0x598 miNextVRegId -- monotonic vreg-id counter (read + post-incremented,
//                          the pre-increment value becomes VRegInfo::miId).
//   +0x5AC mpArena      -- the arena every graph object (VRegInfo, its use/def
//                          vectors, its def stack) is carved from.
// FLAG: everything before +0x598 and between the two fields is opaque padding
// (no DWARF / reference); only these two offsets are grounded in the asm.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsArena.h" // XGRAPHICS::Arena

namespace XGRAPHICS
{

class CFG
{
public:
    u8     maPad000[0x598];            // +0x000 .. +0x597 opaque (owned by the full CFG TU)
    s32    miNextVRegId;               // +0x598  next virtual-register id (post-incremented)
    u8     maPad59C[0x5AC - 0x59C];    // +0x59C .. +0x5AB opaque
    Arena* mpArena;                    // +0x5AC  arena all graph objects allocate from
};

} // namespace XGRAPHICS
