#pragma once

// ===========================================================================
// XGRAPHICS::IRAllocInterp -- the "alloc interp" node kind in the XGRAPHICS
// shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It
// IS-A IRInst: like the other IR node kinds it splices itself into a Block's
// instruction lists and is torn down through the shared IR-node teardown idiom
// (the vector-deleting-destructor thunk restamps the base IRInst vtable to
// &off_821AEB78, then frees the arena-prefixed storage via Arena::Free(*(this-1))
// when the deleting flag is set). This header is the canonical OWNING home for
// the class TU IRAllocInterp:
//
//     XGRAPHICS::IRAllocInterp::InstType                     @ 0x82C2B720
//     XGRAPHICS::IRAllocInterp::`vector deleting destructor' @ 0x82C2B730 (thunk)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm, mirroring the committed IR-instruction
// siblings IRLoadInterp / IRFetch (both homed against IRInst). Only the two
// methods this TU defines are declared here; IRAllocInterp adds no members of
// its own, and its ctor / arena factory (which stamp the node's own vtable) live
// in a separate TU, so they are intentionally NOT declared here. `XGRAPHICS` is
// an X360 graphics-SDK boundary, so its identifiers are preserved verbatim per
// the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

struct IRAllocInterp : public IRInst
{
    // @ 0x82C2B730 -- destructor. The vector-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then routes the storage back to XGRAPHICS::Arena via Arena::Free(*(this-1))
    // when the deleting flag is set) and is dropped per project policy; the
    // destructor proper performs no member teardown in the asm, so the class
    // destructor is trivial.
    virtual ~IRAllocInterp();

    // @ 0x82C2B720 -- instruction-type tag string ("alloc_interp").
    const char* InstType();
};

} // namespace XGRAPHICS
