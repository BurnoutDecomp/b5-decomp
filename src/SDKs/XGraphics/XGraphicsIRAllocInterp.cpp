#include "SDKs/XGraphics/XGraphicsIRAllocInterp.h"

// ===========================================================================
// XGRAPHICS::IRAllocInterp -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRAllocInterp.h
// for the shape notes. Mirrors the committed IR-instruction sibling
// XGraphicsIRLoadInterp.
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x82C2B730 -- the vector deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then Arena::Free(*(this-1))
// when the deleting flag is set); per project policy deleting-destructor thunks
// are dropped, not written. The destructor proper performs no member teardown in
// the asm, so the class destructor is trivial.
IRAllocInterp::~IRAllocInterp()
{
}

// @ 0x82C2B720 -- returns the instruction-type tag string "alloc_interp".
//   lis   r11, aAllocInterp@ha
//   addi  r3, r11, aAllocInterp@l   ; "alloc_interp"
//   blr
const char* IRAllocInterp::InstType()
{
    return "alloc_interp";
}

} // namespace XGRAPHICS
