#pragma once

// ===========================================================================
// XGRAPHICS::VRegTable -- the shader-microcode compiler's virtual-register table
// in BURNOUT_X360_ARTIST.XEX. The CFG owns one (CFG +0x0AC) and routes every
// register lookup/creation through it. This header is a SEED/PARTIAL home: only
// the API the attested callers exercise is declared here; the table's full
// layout, its ctor, and the Find/Create factory-dispatch path (which returns a
// per-register-type MAKER object) are owned by the VRegTable class TU and are
// NOT reconstructed here. EXTEND this header there; do NOT fork it.
//
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention. The layout is intentionally opaque (the
// only user so far, CFG::SetUpParamGen, holds a VRegTable* and calls a member),
// so no members are declared until a body-owning TU grounds them in the asm.
// ===========================================================================

#include "types.hpp"

namespace XGRAPHICS
{

struct VRegInfo; // virtual-register value node (homed in XGraphicsVReg.h)

class VRegTable
{
public:
    // @ 0x82C266F0 caller (CFG::SetUpParamGen) -- find the vreg of register type
    // `aiType` / index `aiIndex`, creating it if absent, and return it. Body is
    // owned by the VRegTable class TU; declared here so its callers compile.
    VRegInfo* FindOrCreate(s32 aiType, s32 aiIndex);
};

} // namespace XGRAPHICS
