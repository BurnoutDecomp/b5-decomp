#include "SDKs/XGraphics/XGraphicsIRLoopIndex.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena source)

// ===========================================================================
// XGRAPHICS::IRLoopIndex -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRLoopIndex.h
// for the shape notes. Each store / branch below is store-for-store with the
// X360 asm, mirroring the committed IR-instruction siblings IRAllocMem /
// IRAllocColor.
// ===========================================================================

namespace XGRAPHICS
{

// The opcode the IRLoopIndex ctor invokes the base IRInst ctor with. NewInst
// hardcodes it (li r4, 0x7E) rather than threading its `aiOpcode` argument.
static const s32 KI_OPCODE_LOOP_INDEX = 126; // 0x7E

// @ 0x82C2C380 (ctor body, inlined into NewInst) -- construct a loop-index node.
//   IRInst::IRInst(this, 126, context); // base ctor + implicit vtable stamp
//   this->miNumOutputs = 1;             // stw 1, 0x10(r3)
//   *this              = &off_821B2528; // implicit IRLoopIndex vtable stamp, stw 0(r3)
//   this->miNumInputs  = 1;             // stw 1, 0x14(r3)
// Note: unlike the IRAlloc* siblings, the asm has NO stw 0,0xA8 -- muUnkA8 is
// left as the base ctor leaves it.
IRLoopIndex::IRLoopIndex(CFG* apContext)
    : IRInst(KI_OPCODE_LOOP_INDEX)
{
    // apContext is live in the base-ctor argument register in the asm (r5) but the
    // 1-arg IRInst base consumes only the opcode; this node stores nothing from it
    // (its arena binding is resolved in NewInst before construction), matching the
    // committed IRAllocMem / IRAllocColor siblings.
    (void)apContext;

    miNumOutputs = 1; // +0x10  the loop-index op produces exactly one value
    miNumInputs  = 1; // +0x14  and consumes one source operand (the loop)
}

// @ 0x82C2B6C0 -- returns the instruction-type tag string "ir_loopindex".
const char* IRLoopIndex::InstType()
{
    return "ir_loopindex";
}

// @ 0x82C2B6D0 -- the vector deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then, on the deleting flag,
// frees the arena-prefixed block via ArenaFreePrefixed -- Arena::Free(*(this-1)));
// per project policy deleting-destructor thunks are dropped, not written. The
// destructor proper performs no member teardown in the asm.
IRLoopIndex::~IRLoopIndex()
{
}

// @ 0x82C2C380 -- read the owning context's arena (apContext->mpArena, +0x5AC),
// carve a prefixed IRLoopIndex block from it (0x3C4 bytes = object + Arena*
// prefix) and construct one in place. ArenaAllocPrefixed yields null on the X360
// Malloc -4 OOM sentinel; construct only on success. The X360 stores the arena
// into the block's leading slot before the OOM test (stw r30,0(r11) precedes the
// beq); ArenaAllocPrefixed folds that same prefix store + sentinel check. The
// factory ABI carries an opcode argument, but IRLoopIndex hardcodes opcode 0x7E
// in the ctor, so `aiOpcode` is unused here (matching the asm, which never reads
// its first argument register).
IRLoopIndex* IRLoopIndex::NewInst(s32 aiOpcode, CFG* apContext)
{
    (void)aiOpcode; // opcode is intrinsic to the node kind (0x7E); arg ignored

    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRLoopIndex));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRLoopIndex(apContext);
}

} // namespace XGRAPHICS
