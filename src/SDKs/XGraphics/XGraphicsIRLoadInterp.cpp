#include "SDKs/XGraphics/XGraphicsIRLoadInterp.h"

// ===========================================================================
// XGRAPHICS::IRLoadInterp -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRLoadInterp.h
// for the shape notes. Each store below is store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

// The IRLoadInterp opcode the base IRInst ctor is invoked with (r4 = 0x7A).
static const s32 KI_OPCODE_LOAD_INTERP = 122;

// The node-flags bit stamped for an interpolated load (ori r10, r10, 0x40).
static const u32 KU_LOAD_INTERP_FLAG = 0x40;

// @ 0x82C2B4E0 -- construct a load-interp node over the compiler context.
//   IRInst::IRInst(this, 122, apContext); // base ctor + implicit vtable stamp
//   this->muUnkE4      |= 0x40;           // lwz 0xE4; ori 0x40; stw 0xE4
//   *this               = &off_821B23C8;  // implicit IRLoadInterp vtable stamp
//   this->miNumOutputs  = 1;              // stw 1, 0x10(r3)
//   this->miNumInputs   = 0;              // stw 0, 0x14(r3)
IRLoadInterp::IRLoadInterp(CFG* apContext)
    : IRInst(KI_OPCODE_LOAD_INTERP)
{
    // apContext is live in the base-ctor argument register (r5) in the asm but
    // the 1-arg IRInst base consumes only the opcode; this node stores nothing
    // from it (same idiom as IRMov's context param).
    (void)apContext;

    muUnkE4     |= KU_LOAD_INTERP_FLAG; // +0xE4  mark load as interpolated
    miNumOutputs = 1;                   // +0x10
    miNumInputs  = 0;                   // +0x14
}

// @ 0x82C2B540 -- the vector deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then Arena::Free(*(this-1))
// when the deleting flag is set); per project policy deleting-destructor thunks
// are dropped, not written. The destructor proper performs no member teardown in
// the asm, so the class destructor is trivial.
IRLoadInterp::~IRLoadInterp()
{
}

// @ 0x82C2B530 -- returns the instruction-type tag string "load interp".
const char* IRLoadInterp::InstType()
{
    return "load interp";
}

} // namespace XGRAPHICS
