#include "SDKs/XGraphics/XGraphicsIRProjection.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena source)

// ===========================================================================
// XGRAPHICS::IRProjection -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRProjection.h
// for the shape notes (including the six vtable slots IRProjection overrides and
// the two cross-TU blockers this file cannot fix on its own). Each store / branch
// below is store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

// The fixed opcode the projection node's IRInst base ctor is invoked with
// (r4 = 0x80 in the NewInst asm).
static const s32 KI_OPCODE_PROJECTION = 128;

// @ 0x82C2C2B0 (folded into NewInst) -- construct a projection node.
//   IRInst::IRInst(this, 128, apContext);  // 3-arg base ctor, r5 = context
//   *this              = &off_821B2440;    // implicit IRProjection vtable stamp, stw 0(r3)
//   this->miNumOutputs = 1;                // stw 1, 0x10(r3)
//   this->miNumInputs  = 1;                // stw 1, 0x14(r3)
IRProjection::IRProjection(CFG* apContext)
    : IRInst(KI_OPCODE_PROJECTION)
{
    // BLOCKED (cross-TU): the asm passes apContext to the base ctor in r5 and the
    // base ctor at 0x82C2B030 CONSUMES it -- it takes the node id from the graph's
    // node counter at context +0x560, post-increments that counter, and stores the
    // context pointer into the node at +0x3B8. The locally reconstructed IRInst
    // declares only `IRInst(s32)` (XGraphicsIRInst.h; class:XGRAPHICS::IRInst is a
    // separate, `blocked` TU that is not editable from here), so the context
    // cannot be forwarded yet. Re-doing the base ctor's stores here instead would
    // misattribute them to the derived ctor, so the forward is left undone and
    // reported. Dropping apContext is NOT what the console does.
    (void)apContext;

    miNumOutputs = 1; // +0x10
    miNumInputs  = 1; // +0x14
}

// @ 0x82C2B5A0 -- the scalar deleting destructor is a compiler-generated thunk: it
// stamps the DListNode base vtable &off_821AEB78 (the intermediate IRInst vtable
// stamp is dead-store eliminated -- IRInst's own thunk at 0x82C28648 writes the
// same word), then, on the deleting flag, frees the arena-prefixed block via
// ArenaFreePrefixed -- Arena::Free(*(this-1)). Per project policy deleting-
// destructor thunks are dropped, not written. The destructor proper performs no
// member teardown in the asm.
IRProjection::~IRProjection()
{
}

// @ 0x82C2B590 -- returns the instruction-type tag string "ir_projection" (the
// literal lives at 0x821B24A8, immediately after IRProjection's vtable). This is
// vtable slot 23 on console; see the header for why it cannot be marked virtual
// until IRInst::InstType is.
const char* IRProjection::InstType()
{
    return "ir_projection";
}

// @ 0x82C2C2B0 -- carve a prefixed IRProjection block from the context's arena and
// construct one in place. The asm reads the arena from the context (lwz r30,
// 0x5AC(r31) = CFG::mpArena), Malloc's object+prefix (li r4, 0x3C4 == 964 == console
// sizeof(IRProjection) 960 PLUS the 4-byte arena prefix -- addic. r3, r11, 4 makes the
// OBJECT pointer r11+4 and stw r30, 0(r11) fills the prefix, so the object occupies
// [r11+4, r11+0x3C4) = 0x3C0 = 960 bytes; the host reconstruction must pass host
// sizeof to ArenaAllocPrefixed, which adds the prefix itself -- never this immediate,
// and never 964 as an object size), stores the owning arena into the prefix
// slot unconditionally, and skips construction on the X360 Malloc -4 OOM sentinel
// (addic. r3, r11, 4 / beq) -- all of which is the ArenaAllocPrefixed idiom. The
// dispatch `aiOpcode` arg is ignored: the projection opcode is hard-coded to 128
// in the ctor (li r4, 0x80).
IRProjection* IRProjection::NewInst(s32 aiOpcode, CFG* apContext)
{
    (void)aiOpcode; // projection's opcode is fixed at 128 (see ctor); dispatch arg unused

    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRProjection));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRProjection(apContext);
}

} // namespace XGRAPHICS
