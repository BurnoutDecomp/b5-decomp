#include "SDKs/XGraphics/XGraphicsIRAllocColor.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena source)

// ===========================================================================
// XGRAPHICS::IRAllocColor -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRAllocColor.h
// for the shape notes. Each store / branch below is store-for-store with the
// X360 asm, mirroring the committed IR-instruction siblings IRZeroary / IRNop
// and the direct twin IRAllocPos.
// ===========================================================================

namespace XGRAPHICS
{

// The opcode the IRAllocColor ctor invokes the base IRInst ctor with. NewInst
// hardcodes it (li r4, 0x90) rather than threading its `aiOpcode` argument.
static const s32 KI_OPCODE_ALLOC_COLOR = 144; // 0x90

// @ 0x82C2C510 (ctor body, inlined into NewInst) -- construct an alloc-colour node.
//   IRInst::IRInst(this, 144, context); // base ctor + implicit vtable stamp
//   this->muUnkA8      = 0;             // stw 0, 0xA8(r3)
//   this->miNumInputs  = 0;             // stw 0, 0x14(r3)
//   this->miNumOutputs = 1;             // stw 1, 0x10(r3)
//   *this              = &off_821B2708; // implicit IRAllocColor vtable stamp, stw 0(r3)
IRAllocColor::IRAllocColor(CFG* apContext)
    : IRInst(KI_OPCODE_ALLOC_COLOR)
{
    // apContext is live in the base-ctor argument register in the asm (r5) but the
    // 1-arg IRInst base consumes only the opcode; this node stores nothing from it
    // (the alloc node's context binding is not part of the ctor), matching the
    // committed IRZeroary / IRAllocPos siblings.
    (void)apContext;

    muUnkA8      = 0; // +0xA8  FLAG node word, zero-stamped (stw 0,0xA8(r3))
    miNumInputs  = 0; // +0x14  an alloc op takes no source operands
    miNumOutputs = 1; // +0x10  and produces exactly one value (the reserved reg)
}

// @ 0x82C2B840 -- returns the instruction-type tag string "alloc_color".
const char* IRAllocColor::InstType()
{
    return "alloc_color";
}

// @ 0x82C2B850 -- the scalar deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then, on the deleting flag,
// frees the arena-prefixed block via ArenaFreePrefixed -- Arena::Free(*(this-1)));
// per project policy deleting-destructor thunks are dropped, not written. The
// destructor proper performs no member teardown in the asm.
IRAllocColor::~IRAllocColor()
{
}

// @ 0x82C2C510 -- read the owning context's arena (apContext->mpArena, +0x5AC),
// carve a prefixed IRAllocColor block from it (0x3C4 bytes = object + Arena*
// prefix) and construct one in place. ArenaAllocPrefixed yields null on the X360
// Malloc -4 OOM sentinel; construct only on success. The X360 stores the arena
// into the block's leading slot before the OOM test (stw r30,0(r11) precedes the
// beq); ArenaAllocPrefixed folds that same prefix store + sentinel check. The
// factory ABI carries an opcode argument, but IRAllocColor hardcodes opcode 0x90
// in the ctor, so `aiOpcode` is unused here (matching the asm, which never reads
// its first argument register).
IRAllocColor* IRAllocColor::NewInst(s32 aiOpcode, CFG* apContext)
{
    (void)aiOpcode; // opcode is intrinsic to the node kind (0x90); arg ignored

    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRAllocColor));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRAllocColor(apContext);
}

} // namespace XGRAPHICS
