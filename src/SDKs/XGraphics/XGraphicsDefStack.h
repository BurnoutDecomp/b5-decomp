#pragma once

// ===========================================================================
// XGRAPHICS::DefStack -- the per-virtual-register definition stack the shader
// microcode compiler threads while building def-use chains (one is owned by
// every XGRAPHICS::VRegInfo, at VRegInfo+0x2C). It is allocated from the graph
// arena with a leading owner-arena pointer (see ArenaAllocPrefixed) and freed
// the same way in ~VRegInfo.
//
// The constructor shape below is attested from the INLINED ctor in
// VRegInfo::VRegInfo (two stores: the owning CFG context, then a zeroed top
// index). The destructor body and the push/pop operations are a separate TU
// (XGRAPHICS::DefStack::~DefStack @ 0x82C...); this header declares the dtor so
// ~VRegInfo can invoke it under the per-TU compile gate. `XGRAPHICS` is an X360
// graphics-SDK boundary, so its identifiers are preserved verbatim per the
// naming convention.
//
// ATTESTED members (X360 byte displacements from VRegInfo::VRegInfo):
//   +0x00 mpContext -- the owning CFG/compiler context (ctor arg).
//   +0x04 miTop     -- stack cursor, initialised to 0. FLAG: name inferred from
//                      the "DefStack" role; only the zero-init is attested.
// ===========================================================================

#include "types.hpp"

namespace XGRAPHICS
{

class CFG; // owning compiler context (homed in XGraphicsCFG.h)

class DefStack
{
public:
    // Attested inlined ctor: stash the owning context, start the stack empty.
    explicit DefStack(CFG* apContext) : mpContext(apContext), miTop(0) {}

    // @ separate TU -- declared only (resolved at link; body not this TU's work).
    ~DefStack();

    CFG* mpContext; // +0x00 owning CFG/compiler context
    s32  miTop;     // +0x04 stack cursor (Init 0)  FLAG name inferred
};

} // namespace XGRAPHICS
