#include "SDKs/XGraphics/XGraphicsIRCmp1S.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena source)

// ===========================================================================
// XGRAPHICS::IRCmp1S -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX. The
// PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRCmp1S.h for the
// shape notes. Each store / branch below is store-for-store with the X360 asm,
// EXCEPT the ctor -- whose body is out-of-line and not present in this TU (see the
// FLAG on IRCmp1S::IRCmp1S below).
// ===========================================================================

namespace XGRAPHICS
{

// @ (out-of-line target of NewInst's `bl XGRAPHICS::IRCmp1S::IRCmp1S`) -- construct
// a scalar-compare node.
//
// FLAG: this ctor is NOT one of the IRCmp1S TU's three functions; it is called by
// NewInst @0x82C2C1C0 through an out-of-line branch and is marked external/unknown
// in this TU's postmortem, so its asm (and therefore its internal operand/count
// stores) is not available here. Unlike the IRUnary / IRBinary / IRProjection
// siblings -- whose ctors the compiler folded INTO NewInst, exposing their
// miNumOutputs / miNumInputs stores in the factory asm -- IRCmp1S's ctor is a
// separate function and reveals nothing beyond its `(this, opcode, context)`
// signature (attested only by the NewInst call site). Rather than fabricate the
// node's output/input counts or operand bindings, this reconstruction models ONLY
// the grounded part: delegation to the IRInst(opcode) base ctor (which stamps the
// DListNode/IRInst fields) plus the implicit IRCmp1S vtable stamp the compiler
// emits for a class with a virtual dtor. The scalar-compare-specific member stores
// live in the un-recovered ctor TU and are intentionally left un-modeled.
IRCmp1S::IRCmp1S(s32 aiOpcode, CFG* apContext)
    : IRInst(aiOpcode)
{
    // apContext is live in the base-ctor argument register (r5) at the NewInst call
    // site but the 1-arg IRInst base consumes only the opcode; mirror the sibling
    // nodes, which store nothing from it. (The ctor's own body is external/unknown;
    // see the FLAG above.)
    (void)apContext;
}

// @ 0x82C2BD28 -- the scalar deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then, on the deleting flag,
// frees the arena-prefixed block via ArenaFreePrefixed -- Arena::Free(*(this-1)));
// per project policy deleting-destructor thunks are dropped, not written. The
// destructor proper performs no member teardown in the asm, so the class
// destructor is trivial.
IRCmp1S::~IRCmp1S()
{
}

// @ 0x82C2BD18 -- returns the instruction-type tag string "ir_cmp1s"
// (lis/addi aIrCmp1s; blr).
const char* IRCmp1S::InstType()
{
    return "ir_cmp1s";
}

// @ 0x82C2C1C0 -- carve a prefixed IRCmp1S block from the context's arena and
// construct one in place.
//   v4 = context->mpArena;                    // lwz 0x5AC(r30)
//   v5 = XGRAPHICS::Arena::Malloc(v4, 0x3C4); // object (0x3C0) + 4-byte prefix
//   *v5 = v4;                                 // store owning arena into prefix slot
//   if (v5 == -4) return 0;                   // X360 Malloc OOM sentinel -> null
//   return IRCmp1S::IRCmp1S(v5 + 1, opcode, context); // construct just past prefix
// The Malloc(size, 0x3C4) / prefix-store / -4 OOM check is exactly the
// ArenaAllocPrefixed idiom; the literal 0x3C4 is the X360 object size + 4-byte
// prefix, so the PC recon uses sizeof(IRCmp1S) (semantic parity, not byte-match),
// matching the IRBinary / IRUnary factories.
IRCmp1S* IRCmp1S::NewInst(s32 aiOpcode, CFG* apContext)
{
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRCmp1S));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRCmp1S(aiOpcode, apContext);
}

} // namespace XGRAPHICS
